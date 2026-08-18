#!/usr/bin/env python3
"""Convert, validate and sync Braille Buddy audio assets."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import tempfile
from pathlib import Path


ITEMS = [
    ("a", 1), ("i", 1), ("u", 1), ("e", 1), ("o", 1),
    ("ka", 2), ("ki", 2), ("ku", 2), ("ke", 2), ("ko", 2),
    ("sa", 3), ("shi", 3), ("su", 3), ("se", 3), ("so", 3),
    ("ta", 4), ("chi", 4), ("tsu", 4), ("te", 4), ("to", 4),
    ("na", 5), ("ni", 5), ("nu", 5), ("ne", 5), ("no", 5),
    ("ha", 6), ("hi", 6), ("fu", 6), ("he", 6), ("ho", 6),
    ("ma", 7), ("mi", 7), ("mu", 7), ("me", 7), ("mo", 7),
    ("ya", 8), ("yu", 8), ("yo", 8),
    ("ra", 9), ("ri", 9), ("ru", 9), ("re", 9), ("ro", 9),
    ("wa", 10), ("wo", 10), ("n", 10),
    ("small_tsu", 10), ("long_vowel", 10),
]

SOURCE_EXTENSIONS = {".wav", ".mp3", ".m4a", ".aac", ".flac", ".ogg"}
TARGET_CODEC = "adpcm_ima_wav"
TARGET_SAMPLE_RATE = 12000
TARGET_CHANNELS = 1
DEFAULT_MAX_MIB = 12.0


def expected_catalog() -> dict[str, set[str]]:
    catalog: dict[str, set[str]] = {
        "system": {
            "power_on_intro.wav",
            "welcome_back.wav",
            "unknown_pattern.wav",
            "goodbye.wav",
            "no_learned_items.wav",
        },
        "study": {
            "study_intro.wav",
            "study_next_or_repeat.wav",
            "continue_next.wav",
            "repeat_lesson.wav",
        },
        "lessons": set(),
        "chars": set(),
        "hints": set(),
        "practice": {"practice_intro.wav"},
    }

    for lesson in range(1, 11):
        catalog["lessons"].add(f"lesson{lesson:02d}_title.wav")
        catalog["lessons"].add(f"lesson{lesson:02d}_story.wav")

    for item_id, lesson in ITEMS:
        catalog["lessons"].add(f"lesson{lesson:02d}_{item_id}.wav")
        catalog["chars"].add(f"char_{item_id}.wav")
        catalog["hints"].add(f"hint_{item_id}.wav")

    for index in range(1, 6):
        catalog["practice"].add(f"quiz_intro_{index:02d}.wav")
    for index in range(1, 16):
        suffix = f"0{index}" if index >= 10 else f"{index:02d}"
        catalog["practice"].add(f"correct_{suffix}.wav")
    for level in (1, 2, 3):
        for index in range(1, 11):
            suffix = f"0{index}" if index >= 10 else f"{index:02d}"
            catalog["practice"].add(f"wrong_{level:02d}_{suffix}.wav")
    return catalog


def build_name_index() -> dict[str, str]:
    result: dict[str, str] = {}
    for category, names in expected_catalog().items():
        for name in names:
            result[name.lower()] = category
    return result


def validate_wav(path: Path) -> tuple[bool, str]:
    if shutil.which("ffprobe") is None:
        return False, "không tìm thấy ffprobe trong PATH"

    try:
        completed = subprocess.run(
            [
                "ffprobe", "-v", "error", "-select_streams", "a:0",
                "-show_entries",
                "stream=codec_name,sample_rate,channels,bit_rate,duration",
                "-of", "json", str(path),
            ],
            check=True,
            capture_output=True,
            text=True,
            encoding="utf-8",
        )
        payload = json.loads(completed.stdout)
    except (
        json.JSONDecodeError,
        OSError,
        subprocess.CalledProcessError,
    ) as error:
        return False, f"ffprobe không đọc được WAV: {error}"

    streams = payload.get("streams", [])
    if not streams:
        return False, "không tìm thấy audio stream"

    stream = streams[0]
    codec = stream.get("codec_name", "")
    try:
        sample_rate = int(stream.get("sample_rate", 0))
        channels = int(stream.get("channels", 0))
        duration = float(stream.get("duration", 0.0))
    except (TypeError, ValueError) as error:
        return False, f"metadata audio không hợp lệ: {error}"

    if codec != TARGET_CODEC:
        return False, f"codec={codec or 'không rõ'}, cần {TARGET_CODEC}"
    if channels != TARGET_CHANNELS:
        return False, f"số kênh={channels}, cần mono"
    if sample_rate != TARGET_SAMPLE_RATE:
        return False, (
            f"sample rate={sample_rate}, cần {TARGET_SAMPLE_RATE} Hz"
        )
    if duration <= 0.0 or path.stat().st_size <= 0:
        return False, "file không có mẫu âm thanh"
    return True, "OK"


def command_convert(args: argparse.Namespace) -> int:
    if shutil.which("ffmpeg") is None:
        print("Không tìm thấy ffmpeg trong PATH.")
        return 2
    if shutil.which("ffprobe") is None:
        print("Không tìm thấy ffprobe trong PATH.")
        return 2

    source_root = args.source.resolve()
    output_root = args.output.resolve()
    name_index = build_name_index()
    converted = 0
    skipped = 0

    for source in sorted(source_root.rglob("*")):
        if not source.is_file() or source.suffix.lower() not in SOURCE_EXTENSIONS:
            continue

        target_name = source.with_suffix(".wav").name.lower()
        category = name_index.get(target_name)
        if category is None:
            print(f"Bỏ qua tên không có trong báo cáo: {source.name}")
            skipped += 1
            continue

        target_dir = output_root / category
        target_dir.mkdir(parents=True, exist_ok=True)
        target = target_dir / target_name

        with tempfile.NamedTemporaryFile(
            suffix=".wav", dir=target_dir, delete=False
        ) as temp_file:
            temporary = Path(temp_file.name)

        try:
            subprocess.run(
                [
                    "ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
                    "-i", str(source), "-ac", str(TARGET_CHANNELS),
                    "-ar", str(TARGET_SAMPLE_RATE),
                    "-c:a", TARGET_CODEC, "-map_metadata", "-1",
                    str(temporary),
                ],
                check=True,
            )
            valid, message = validate_wav(temporary)
            if not valid:
                print(f"Lỗi {source.name}: {message}")
                continue
            temporary.replace(target)
            converted += 1
        except subprocess.CalledProcessError:
            print(f"FFmpeg không chuyển được: {source}")
        finally:
            temporary.unlink(missing_ok=True)

    print(f"Đã chuyển đổi: {converted}; bỏ qua: {skipped}")
    return command_validate(
        argparse.Namespace(
            audio_root=output_root,
            max_mib=args.max_mib,
            ignore_missing=args.ignore_missing,
        )
    )


def command_validate(args: argparse.Namespace) -> int:
    if shutil.which("ffprobe") is None:
        print("Không tìm thấy ffprobe trong PATH.")
        return 2

    audio_root = args.audio_root.resolve()
    catalog = expected_catalog()
    missing: list[Path] = []
    invalid: list[tuple[Path, str]] = []
    total_size = 0

    for category, names in catalog.items():
        for name in sorted(names):
            path = audio_root / category / name
            if not path.exists():
                missing.append(path.relative_to(audio_root))
                continue
            valid, message = validate_wav(path)
            if not valid:
                invalid.append((path.relative_to(audio_root), message))
            total_size += path.stat().st_size

    print(f"Tổng dung lượng WAV: {total_size / (1024 * 1024):.2f} MiB")
    print(f"Thiếu: {len(missing)}; không hợp lệ: {len(invalid)}")
    if not args.ignore_missing:
        for path in missing:
            print(f"THIẾU  {path}")
    for path, message in invalid:
        print(f"LỖI    {path}: {message}")

    if total_size > args.max_mib * 1024 * 1024:
        print(
            f"CẢNH BÁO: vượt {args.max_mib:.1f} MiB. "
            "Hãy kiểm tra kích thước phân vùng LittleFS hoặc dùng thẻ microSD."
        )
    if missing and args.ignore_missing:
        print(
            f"Đã bỏ qua {len(missing)} file thiếu theo tùy chọn "
            "--ignore-missing."
        )
    return 1 if invalid or (missing and not args.ignore_missing) else 0


def command_sync(args: argparse.Namespace) -> int:
    if shutil.which("ffprobe") is None:
        print("Không tìm thấy ffprobe trong PATH.")
        return 2

    source_root = args.audio_root.resolve()
    destination_root = args.destination.resolve()
    catalog = expected_catalog()
    copied = 0
    invalid = 0

    for category, names in catalog.items():
        destination_dir = destination_root / category
        destination_dir.mkdir(parents=True, exist_ok=True)
        for name in names:
            source = source_root / category / name
            if not source.exists():
                continue
            valid, message = validate_wav(source)
            if not valid:
                print(f"Không chép {source}: {message}")
                invalid += 1
                continue
            shutil.copy2(source, destination_dir / name)
            copied += 1

    print(f"Đã chép {copied} file vào {destination_root}")
    return 1 if invalid else 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Chuẩn hóa và kiểm tra WAV cho Braille Buddy"
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    convert_parser = subparsers.add_parser("convert")
    convert_parser.add_argument("source", type=Path)
    convert_parser.add_argument("--output", type=Path, required=True)
    convert_parser.add_argument(
        "--max-mib", type=float, default=DEFAULT_MAX_MIB
    )
    convert_parser.add_argument("--ignore-missing", action="store_true")
    convert_parser.set_defaults(handler=command_convert)

    validate_parser = subparsers.add_parser("validate")
    validate_parser.add_argument("audio_root", type=Path)
    validate_parser.add_argument(
        "--max-mib", type=float, default=DEFAULT_MAX_MIB
    )
    validate_parser.add_argument("--ignore-missing", action="store_true")
    validate_parser.set_defaults(handler=command_validate)

    sync_parser = subparsers.add_parser("sync")
    sync_parser.add_argument("audio_root", type=Path)
    sync_parser.add_argument("--destination", type=Path, required=True)
    sync_parser.set_defaults(handler=command_sync)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    return args.handler(args)


if __name__ == "__main__":
    raise SystemExit(main())
