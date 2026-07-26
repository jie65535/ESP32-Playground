#include "services/TinyLmService.h"

#include <algorithm>
#include <atomic>
#include <climits>
#include <cmath>
#include <cstring>
#include <new>

#include <esp_heap_caps.h>
#include <esp_partition.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#define LLM_PROFILE 1
#define LLM_PROFILE_NOW() esp_timer_get_time()
#include "third_party/esp32_ai/firmware/common/llm.h"
#include "third_party/esp32_ai/firmware/esp32_llm/vocab.h"

namespace {

constexpr size_t MODEL_BYTES = 3917660;
constexpr uint16_t EOT_TOKEN = 0;
constexpr float TEMPERATURE = 0.8F;
constexpr uint8_t MAX_PROMPT_TOKENS = 8;
constexpr uint8_t TOKEN_QUEUE_DEPTH = 64;

}  // namespace

struct TinyLmService::Impl {
    enum class Control : uint8_t {
        Generate,
        Stop,
        Unload,
    };

    struct Job {
        Control control = Control::Stop;
        uint32_t generation = 0;
        uint16_t prompt[MAX_PROMPT_TOKENS] = {};
        uint8_t promptCount = 0;
        uint16_t maxTokens = 0;
    };

    Print* log = nullptr;
    const esp_partition_t* partition = nullptr;
    const uint8_t* modelBase = nullptr;
    esp_partition_mmap_handle_t mmapHandle = 0;
    bool mapped = false;
    bool modelReady = false;
    bool resourcesReady = false;

    Model model{};
    Scratch scratch{};

    int8_t* headWeights = nullptr;
    float* headScales = nullptr;
    int headRows = 0;
    int headCols = 0;
    int8_t headActivation[256] = {};
    float headActivationScale = 0.0F;
    float* volatile headOutput = nullptr;
    volatile int headSplit = 0;

    QueueHandle_t jobs = nullptr;
    QueueHandle_t tokens = nullptr;
    SemaphoreHandle_t headDone = nullptr;
    TaskHandle_t workerTask = nullptr;
    TaskHandle_t headWorkerTask = nullptr;
    std::atomic<uint32_t> nextGeneration{0};

    mutable portMUX_TYPE snapshotMux = portMUX_INITIALIZER_UNLOCKED;
    TinyLmSnapshot current{};

    static Impl* active;

    void setSnapshot(const TinyLmSnapshot& value) {
        portENTER_CRITICAL(&snapshotMux);
        current = value;
        portEXIT_CRITICAL(&snapshotMux);
    }

    TinyLmSnapshot getSnapshot() const {
        portENTER_CRITICAL(&snapshotMux);
        TinyLmSnapshot value = current;
        portEXIT_CRITICAL(&snapshotMux);
        return value;
    }

    void setError(const char* message) {
        TinyLmSnapshot value = getSnapshot();
        value.state = TinyLmState::Error;
        value.available = modelReady;
        strncpy(value.error, message == nullptr ? "unknown" : message,
                sizeof(value.error) - 1U);
        value.error[sizeof(value.error) - 1U] = '\0';
        value.freePsram = ESP.getFreePsram();
        setSnapshot(value);
        if (log != nullptr) {
            log->print(F("[tinylm] error: "));
            log->println(value.error);
        }
    }

    static void* allocatePsram(size_t bytes) {
        return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }

    static float* allocateFloats(size_t count) {
        return static_cast<float*>(allocatePsram(count * sizeof(float)));
    }

    void freeResources() {
        auto release = [](void*& pointer) {
            if (pointer != nullptr) {
                heap_caps_free(pointer);
                pointer = nullptr;
            }
        };
        void* pointer = headWeights;
        release(pointer);
        headWeights = static_cast<int8_t*>(pointer);
        pointer = headScales;
        release(pointer);
        headScales = static_cast<float*>(pointer);

        float** buffers[] = {
            &scratch.x,      &scratch.h,      &scratch.qkv,   &scratch.att,
            &scratch.g1,     &scratch.g2,     &scratch.ple,   &scratch.tmpP,
            &scratch.trow,   &scratch.logits, &scratch.scores,
            &scratch.kcache, &scratch.vcache,
        };
        for (float** buffer : buffers) {
            pointer = *buffer;
            release(pointer);
            *buffer = static_cast<float*>(pointer);
        }
        memset(&scratch, 0, sizeof(scratch));
        resourcesReady = false;
        if (active == this) {
            active = nullptr;
        }
    }

    bool ensureResources() {
        if (resourcesReady) {
            return true;
        }
        freeResources();
        headRows = model.tok_emb.rows;
        headCols = model.tok_emb.cols;
        if (headCols > static_cast<int>(sizeof(headActivation)) ||
            model.tok_emb.n_groups != 1) {
            setError("unsupported output head layout");
            return false;
        }

        headWeights = static_cast<int8_t*>(
            allocatePsram(static_cast<size_t>(headRows) * headCols));
        headScales = static_cast<float*>(
            allocatePsram(static_cast<size_t>(headRows) * sizeof(float)));
        if (headWeights == nullptr || headScales == nullptr) {
            freeResources();
            setError("PSRAM output head allocation failed");
            return false;
        }
        for (int row = 0; row < headRows; ++row) {
            const uint8_t* source =
                model.tok_emb.codes +
                static_cast<size_t>(row) * model.tok_emb.row_bytes;
            int8_t* destination =
                headWeights + static_cast<size_t>(row) * headCols;
            for (int column = 0; column < headCols; ++column) {
                const uint8_t packed = source[column >> 1];
                destination[column] = static_cast<int8_t>(
                    ((column & 1) ? (packed >> 4) : (packed & 0x0FU)) - 8);
            }
            headScales[row] = half2float(
                model.tok_emb.scales[static_cast<size_t>(row)]);
        }

        const int D = model.c.dim;
        const int L = model.c.n_layers;
        const int P = model.c.ple_dim;
        const int F = model.c.ffn;
        const int V = model.c.vocab;
        const int S = model.c.seq_len;
        scratch.x = allocateFloats(D);
        scratch.h = allocateFloats(std::max(F, D));
        scratch.qkv = allocateFloats(3 * D);
        scratch.att = allocateFloats(D);
        scratch.g1 = allocateFloats(F);
        scratch.g2 = allocateFloats(std::max(P, F));
        scratch.ple = allocateFloats(L * P);
        scratch.tmpP = allocateFloats(L * P);
        scratch.trow = allocateFloats(L * P);
        scratch.logits = allocateFloats(V);
        scratch.scores = allocateFloats(S);
        scratch.kcache =
            allocateFloats(static_cast<size_t>(L) * S * D);
        scratch.vcache =
            allocateFloats(static_cast<size_t>(L) * S * D);

        if (scratch.x == nullptr || scratch.h == nullptr ||
            scratch.qkv == nullptr || scratch.att == nullptr ||
            scratch.g1 == nullptr || scratch.g2 == nullptr ||
            scratch.ple == nullptr || scratch.tmpP == nullptr ||
            scratch.trow == nullptr || scratch.logits == nullptr ||
            scratch.scores == nullptr || scratch.kcache == nullptr ||
            scratch.vcache == nullptr) {
            freeResources();
            setError("PSRAM inference allocation failed");
            return false;
        }

        active = this;
        model.head_matvec = headMatvecBridge;
        resourcesReady = true;
        if (log != nullptr) {
            log->printf("[tinylm] ready head=%d x %d free_psram=%u\n",
                        headRows, headCols,
                        static_cast<unsigned>(ESP.getFreePsram()));
        }
        return true;
    }

    static inline int32_t dotI8(const int8_t* left, const int8_t* right,
                                int count) {
        int32_t sum = 0;
        for (int index = 0; index < count; ++index) {
            sum += static_cast<int32_t>(left[index]) *
                   static_cast<int32_t>(right[index]);
        }
        return sum;
    }

    void headRange(float* output, int first, int last) {
        for (int row = first; row < last; ++row) {
            output[row] =
                static_cast<float>(dotI8(
                    headActivation,
                    headWeights + static_cast<size_t>(row) * headCols,
                    headCols)) *
                headScales[row] * headActivationScale;
        }
    }

    void headMatvec(const float* input, float* output) {
        quantize_act(input, headCols, headActivation, &headActivationScale);
        headOutput = output;
        headSplit = headRows / 2;
        xTaskNotifyGive(headWorkerTask);
        headRange(output, headSplit, headRows);
        xSemaphoreTake(headDone, portMAX_DELAY);
    }

    static void headMatvecBridge(const QT*, const float* input,
                                 float* output) {
        if (active != nullptr) {
            active->headMatvec(input, output);
        }
    }

    static void headWorkerEntry(void* argument) {
        Impl* self = static_cast<Impl*>(argument);
        for (;;) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            float* output = self->headOutput;
            const int split = self->headSplit;
            if (output != nullptr && self->resourcesReady) {
                self->headRange(output, 0, split);
            }
            xSemaphoreGive(self->headDone);
        }
    }

    bool controlPending() const {
        return uxQueueMessagesWaiting(jobs) > 0;
    }

    int sampleToken() {
        float* logits = scratch.logits;
        float maximum = logits[0];
        for (int token = 1; token < VOCAB_N; ++token) {
            if (logits[token] > maximum) {
                maximum = logits[token];
            }
        }
        float sum = 0.0F;
        for (int token = 0; token < VOCAB_N; ++token) {
            logits[token] =
                expf((logits[token] - maximum) / TEMPERATURE);
            sum += logits[token];
        }
        const float random =
            static_cast<float>(esp_random()) / static_cast<float>(UINT32_MAX);
        float cumulative = 0.0F;
        for (int token = 0; token < VOCAB_N; ++token) {
            cumulative += logits[token] / sum;
            if (random < cumulative) {
                return token;
            }
        }
        return VOCAB_N - 1;
    }

    void run(const Job& job) {
        TinyLmSnapshot value = getSnapshot();
        value.state = TinyLmState::Loading;
        value.available = true;
        value.generation = job.generation;
        value.generatedTokens = 0;
        value.maxTokens = job.maxTokens;
        value.elapsedUs = 0;
        value.computeUs = 0;
        value.error[0] = '\0';
        value.freePsram = ESP.getFreePsram();
        setSnapshot(value);

        if (!ensureResources()) {
            return;
        }

        TinyLmTokenEvent dropped;
        while (xQueueReceive(tokens, &dropped, 0) == pdTRUE) {
        }

        value.state = TinyLmState::Generating;
        value.freePsram = ESP.getFreePsram();
        setSnapshot(value);

        int position = 0;
        for (uint8_t index = 0; index < job.promptCount; ++index) {
            if (controlPending()) {
                return;
            }
            llm_forward(&model, job.prompt[index], position++, &scratch);
        }

        llm_profile_reset(&scratch);
        const int64_t startedUs = esp_timer_get_time();
        uint64_t computeUs = 0;
        uint16_t generated = 0;
        bool reachedEot = false;
        const uint16_t limit = std::min<uint16_t>(
            job.maxTokens,
            static_cast<uint16_t>(model.c.seq_len - position));

        for (uint16_t step = 0; step < limit; ++step) {
            if (controlPending()) {
                return;
            }
            const int token = sampleToken();
            if (token == EOT_TOKEN) {
                reachedEot = true;
                break;
            }

            TinyLmTokenEvent event{
                job.generation, static_cast<uint16_t>(token)};
            if (xQueueSend(tokens, &event, pdMS_TO_TICKS(100)) != pdTRUE) {
                setError("token queue stalled");
                return;
            }

            const int64_t computeStartedUs = esp_timer_get_time();
            llm_forward(&model, token, position++, &scratch);
            computeUs += static_cast<uint64_t>(
                esp_timer_get_time() - computeStartedUs);
            ++generated;

            value = getSnapshot();
            value.generatedTokens = generated;
            value.elapsedUs =
                static_cast<uint64_t>(esp_timer_get_time() - startedUs);
            value.computeUs = computeUs;
            value.freePsram = ESP.getFreePsram();
            setSnapshot(value);
            vTaskDelay(1);
        }

        value = getSnapshot();
        value.state = TinyLmState::Complete;
        value.generatedTokens = generated;
        value.elapsedUs =
            static_cast<uint64_t>(esp_timer_get_time() - startedUs);
        value.computeUs = computeUs;
        value.freePsram = ESP.getFreePsram();
        setSnapshot(value);
        if (log != nullptr) {
            log->printf(
                "[tinylm] complete generation=%u tokens=%u eot=%s "
                "compute=%.2f tok/s\n",
                static_cast<unsigned>(job.generation),
                static_cast<unsigned>(generated), reachedEot ? "yes" : "no",
                computeUs == 0
                    ? 0.0F
                    : static_cast<float>(generated) * 1000000.0F /
                          static_cast<float>(computeUs));
        }
    }

    static void workerEntry(void* argument) {
        Impl* self = static_cast<Impl*>(argument);
        Job job;
        for (;;) {
            if (xQueueReceive(self->jobs, &job, portMAX_DELAY) != pdTRUE) {
                continue;
            }
            switch (job.control) {
                case Control::Generate:
                    self->run(job);
                    break;
                case Control::Stop: {
                    TinyLmSnapshot value = self->getSnapshot();
                    value.state = TinyLmState::Idle;
                    self->setSnapshot(value);
                    break;
                }
                case Control::Unload: {
                    self->freeResources();
                    TinyLmSnapshot value = self->getSnapshot();
                    value.state = TinyLmState::Idle;
                    value.freePsram = ESP.getFreePsram();
                    self->setSnapshot(value);
                    break;
                }
            }
        }
    }

    bool initialize(Print& output) {
        log = &output;
        jobs = xQueueCreate(1, sizeof(Job));
        tokens = xQueueCreate(TOKEN_QUEUE_DEPTH, sizeof(TinyLmTokenEvent));
        headDone = xSemaphoreCreateBinary();
        if (jobs == nullptr || tokens == nullptr || headDone == nullptr) {
            setError("FreeRTOS primitive allocation failed");
            return false;
        }

        partition = esp_partition_find_first(
            ESP_PARTITION_TYPE_DATA,
            static_cast<esp_partition_subtype_t>(0x40), "model");
        if (partition == nullptr || partition->size < MODEL_BYTES) {
            setError("model partition unavailable");
            return false;
        }
        const void* base = nullptr;
        if (esp_partition_mmap(partition, 0, MODEL_BYTES,
                               ESP_PARTITION_MMAP_DATA, &base,
                               &mmapHandle) != ESP_OK) {
            setError("model mmap failed");
            return false;
        }
        modelBase = static_cast<const uint8_t*>(base);
        mapped = true;

        uint32_t magic = 0;
        int32_t config[8] = {};
        memcpy(&magic, modelBase, sizeof(magic));
        memcpy(config, modelBase + sizeof(magic), sizeof(config));
        if (magic != LLM_MAGIC || config[0] != VOCAB_N ||
            config[1] != 128 || config[2] != 6 || config[3] != 4 ||
            config[4] != 512 || config[5] != 96 || config[6] != 512 ||
            config[7] != 128 || llm_load(modelBase, &model) != 0) {
            setError("model header mismatch");
            return false;
        }

        if (xTaskCreatePinnedToCore(headWorkerEntry, "tinylm_head", 4096,
                                    this, 2, &headWorkerTask, 0) != pdPASS) {
            setError("TinyLM task creation failed");
            return false;
        }
        if (xTaskCreatePinnedToCore(workerEntry, "tinylm", 8192, this, 1,
                                    &workerTask, 1) != pdPASS) {
            vTaskDelete(headWorkerTask);
            headWorkerTask = nullptr;
            setError("TinyLM task creation failed");
            return false;
        }

        modelReady = true;
        TinyLmSnapshot value;
        value.state = TinyLmState::Idle;
        value.available = true;
        value.freePsram = ESP.getFreePsram();
        setSnapshot(value);
        output.printf(
            "[tinylm] model ready V=%d D=%d L=%d F=%d P=%d S=%d size=%.2f MB\n",
            model.c.vocab, model.c.dim, model.c.n_layers, model.c.ffn,
            model.c.ple_dim, model.c.seq_len, MODEL_BYTES / 1000000.0F);
        return true;
    }

    void enqueue(Control control, const uint16_t* promptIds = nullptr,
                 size_t promptCount = 0, uint16_t maxTokens = 0,
                 uint32_t generation = 0) {
        if (jobs == nullptr) {
            return;
        }
        Job job;
        job.control = control;
        job.generation = generation;
        job.promptCount = static_cast<uint8_t>(
            std::min<size_t>(promptCount, MAX_PROMPT_TOKENS));
        job.maxTokens = maxTokens;
        if (promptIds != nullptr && job.promptCount > 0) {
            memcpy(job.prompt, promptIds,
                   job.promptCount * sizeof(job.prompt[0]));
        }
        xQueueOverwrite(jobs, &job);
    }
};

TinyLmService::Impl* TinyLmService::Impl::active = nullptr;

bool TinyLmService::begin(Print& log) {
    if (impl_ != nullptr) {
        return impl_->getSnapshot().available;
    }
    impl_ = new (std::nothrow) Impl();
    if (impl_ == nullptr) {
        log.println(F("[tinylm] service allocation failed"));
        return false;
    }
    return impl_->initialize(log);
}

uint32_t TinyLmService::start(const uint16_t* promptIds, size_t promptCount,
                              uint16_t maxTokens) {
    if (impl_ == nullptr || promptIds == nullptr || promptCount == 0 ||
        !impl_->getSnapshot().available) {
        return 0;
    }
    const uint32_t generation = impl_->nextGeneration.fetch_add(1) + 1U;
    impl_->enqueue(Impl::Control::Generate, promptIds, promptCount, maxTokens,
                   generation);
    return generation;
}

void TinyLmService::stop() {
    if (impl_ != nullptr) {
        impl_->enqueue(Impl::Control::Stop);
    }
}

void TinyLmService::unload() {
    if (impl_ != nullptr) {
        impl_->enqueue(Impl::Control::Unload);
    }
}

bool TinyLmService::pollToken(TinyLmTokenEvent& event) {
    return impl_ != nullptr && impl_->tokens != nullptr &&
           xQueueReceive(impl_->tokens, &event, 0) == pdTRUE;
}

size_t TinyLmService::decodeToken(uint16_t token, uint8_t* output,
                                  size_t capacity) const {
    if (token >= VOCAB_N || output == nullptr || capacity == 0) {
        return 0;
    }
    const size_t offset = static_cast<size_t>(VOCAB_OFF[token]);
    const size_t length =
        static_cast<size_t>(VOCAB_OFF[token + 1] - VOCAB_OFF[token]);
    const size_t copied = std::min(length, capacity);
    memcpy(output, VOCAB_BLOB + offset, copied);
    return copied;
}

TinyLmSnapshot TinyLmService::snapshot() const {
    return impl_ == nullptr ? TinyLmSnapshot{} : impl_->getSnapshot();
}
