"""Prepare TinyStories-Zh-1M for training.

Reads local Parquet shards downloaded from
https://huggingface.co/datasets/RobinChen2001/TinyStories-Zh-1M (the ``text``
column), trains a ByteLevel BPE tokenizer, and writes train_zh.bin / val_zh.bin
(uint16 token ids) plus bpe{VOCAB_SIZE}_zh.json.

Usage:
    python prepare_zh.py

By default the script reads the two Parquet shards stored under
``data/raw/TinyStories-Zh-1M``. Pass ``--parquet`` to override them.

Then train with:
    python src/train.py --arm ple --data-suffix _zh --vocab 8192
"""

import argparse
import os
import re
import sys

import numpy as np

HERE   = os.path.dirname(os.path.abspath(__file__))
EOT    = "<|endoftext|>"
DEFAULT_PARQUET = [
    os.path.join(HERE, "raw", "TinyStories-Zh-1M", "train-00000-of-00002.parquet"),
    os.path.join(HERE, "raw", "TinyStories-Zh-1M", "train-00001-of-00002.parquet"),
]
# Heuristic: a story is "untranslated" if it contains a run of 8+ ASCII letters
ASCII_RE = re.compile(r"[A-Za-z]{8,}")
# Meta-commentary phrases that leak from the translation
BAD_PHRASES = ("这个故事的寓意", "故事的主旨", "这个故事告诉我们", "moral of")

VAL_FRACTION = 0.005
TOKENIZER_TRAIN_CHARS = 2_000_000   # 2M Chinese chars (~10K stories) is plenty for BPE


def load_stories(paths):
    try:
        import pyarrow.parquet as pq
    except ImportError:
        sys.exit("pyarrow not found — run: pip install pyarrow")

    stories = []
    for p in paths:
        print(f"reading {p} …")
        tbl = pq.read_table(p, columns=["text"])
        for text in tbl["text"].to_pylist():
            if text:
                stories.append(text)
    print(f"loaded {len(stories):,} stories total")
    return stories


def filter_stories(stories, min_chars=30, max_chars=400):
    kept, dropped = [], 0
    for s in stories:
        s = s.strip()
        if len(s) < min_chars or len(s) > max_chars:
            dropped += 1
            continue
        if ASCII_RE.search(s):
            dropped += 1
            continue
        if any(p in s for p in BAD_PHRASES):
            dropped += 1
            continue
        kept.append(s)
    print(f"filter: kept {len(kept):,}  dropped {dropped:,}")
    return kept


def train_tokenizer(text, vocab_size, out_path):
    from tokenizers import Tokenizer, decoders, models, pre_tokenizers, trainers

    if os.path.exists(out_path):
        print(f"tokenizer already exists: {out_path}")
        from tokenizers import Tokenizer as T
        return T.from_file(out_path)

    slice_text = text[:TOKENIZER_TRAIN_CHARS]
    print(f"training BPE vocab={vocab_size} on {len(slice_text.encode())/1e6:.1f} MB "
          f"({len(slice_text)/1e6:.1f}M chars, {len(slice_text)/max(1,len(text))*100:.1f}% of corpus) …")
    tok = Tokenizer(models.BPE(unk_token=None))
    tok.pre_tokenizer = pre_tokenizers.ByteLevel(add_prefix_space=False)
    tok.decoder = decoders.ByteLevel()
    trainer = trainers.BpeTrainer(
        vocab_size=vocab_size,
        special_tokens=[EOT],
        initial_alphabet=pre_tokenizers.ByteLevel.alphabet(),
        show_progress=True,
    )
    tok.train_from_iterator([slice_text], trainer=trainer)
    tok.save(out_path)
    print(f"saved tokenizer → {out_path}")
    return tok


def encode_stories(tok, stories, eot_id):
    ids = []
    batch_size = 5000
    for i in range(0, len(stories), batch_size):
        batch = stories[i : i + batch_size]
        for enc in tok.encode_batch(batch):
            ids.extend(enc.ids)
            ids.append(eot_id)
        if (i // batch_size) % 10 == 0:
            print(f"  encoded {min(i+batch_size, len(stories)):,}/{len(stories):,} "
                  f"stories  {len(ids)/1e6:.1f}M tokens", flush=True)
    return ids


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--parquet", nargs="+", default=DEFAULT_PARQUET,
                    help="Path(s) to parquet files "
                         "(defaults to data/raw/TinyStories-Zh-1M)")
    ap.add_argument("--vocab", type=int, default=8192,
                    help="BPE vocab size (default 8192)")
    ap.add_argument("--min-chars", type=int, default=30)
    ap.add_argument("--max-chars", type=int, default=400)
    args = ap.parse_args()

    tok_path   = os.path.join(HERE, f"bpe{args.vocab}_zh.json")
    train_path = os.path.join(HERE, "train_zh.bin")
    val_path   = os.path.join(HERE, "val_zh.bin")

    stories = load_stories(args.parquet)
    stories = filter_stories(stories, args.min_chars, args.max_chars)

    # Build raw text for tokenizer training and encoding
    full_text = f" {EOT} ".join(stories)

    tok    = train_tokenizer(full_text, args.vocab, tok_path)
    eot_id = tok.token_to_id(EOT)
    print(f"EOT id = {eot_id}")

    print("encoding stories …")
    ids  = encode_stories(tok, stories, eot_id)
    arr  = np.array(ids, dtype=np.uint16)
    assert arr.max() < args.vocab, f"token id {arr.max()} >= vocab {args.vocab}"

    n_val = max(1, int(len(arr) * VAL_FRACTION))
    arr[:-n_val].tofile(train_path)
    arr[-n_val:].tofile(val_path)

    print(f"train : {len(arr)-n_val:,} tokens → {train_path}")
    print(f"val   : {n_val:,} tokens → {val_path}")
    print(f"bytes/token: {len(full_text.encode())/len(arr):.2f}")
    print(f"\nTo train:  python src/train.py --arm ple --data-suffix _zh --vocab {args.vocab}")


if __name__ == "__main__":
    main()
