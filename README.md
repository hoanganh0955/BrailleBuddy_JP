# BrailleBuddy JP

Firmware ESP32-S3 dạy Braille Hiragana dựa trên `Practice JP-VN.docx`, `Scripts JP-VN.docx` và `Scripts JP.docx`.

## Cấu trúc project

```text
BrailleBuddy_JP/
├── code_cpp/
│   └── BrailleBuddy_JP/
│       ├── BrailleBuddy_JP.ino
│       ├── BrailleApp.h
│       ├── BrailleConfig.h
│       ├── BrailleData.h
│       ├── AudioPlayer.h
│       ├── ImaAdpcmDecoder.h
│       ├── partitions.csv
│       └── data/audio/
├── code_python/
│   ├── braille_cli.py
│   ├── prepare_audio.py
│   └── requirements.txt
└── audio/
    ├── system/
    ├── study/
    ├── lessons/
    ├── chars/
    ├── hints/
    └── practice/
```

## GPIO

| Chức năng | GPIO |
|---|---:|
| Braille D1, D2, D3 | 4, 5, 6 |
| Braille D4, D5, D6 | 7, 15, 16 |
| Practice | 8 |
| Study | 18 |
| Send | 17 |
| MAX98357A DIN | 11 |
| MAX98357A BCLK | 12 |
| MAX98357A LRC/WS | 13 |

Tất cả nút nối giữa GPIO và GND; firmware dùng `INPUT_PULLUP`, nên nhấn là mức `LOW`.

## Logic mới

### Khởi động lần đầu

1. Phát `power_on_intro.wav`.
2. Study mở vòng chọn bài và phát title của bài được đề xuất.
3. Practice bắt đầu luyện tập các chữ đã học.

### Khởi động từ lần thứ hai

1. Firmware đọc khóa `launched` trong Preferences và phát `welcome_back.wav`.
2. Study: mở vòng chọn bài, bắt đầu tại bài chứa chữ chưa học tiếp theo.
3. Practice: học lại từ đầu bài gần nhất.
4. Ngay sau lựa chọn này, Practice trở lại chức năng luyện tập bình thường.

### Study

- Nhấn Study để vào chọn bài. Firmware phát title của bài được đề xuất theo
  tiến độ hiện tại.
- Trong lúc chọn bài, nhấn Study để chuyển tới title bài kế tiếp và nhấn
  Practice để quay về title bài trước. Hai chiều đều tạo thành vòng kín giữa
  bài 1 và bài 10.
- Nhấn Send để xác nhận bài đang nghe và bắt đầu từ chữ đầu tiên của bài đó.
- Mỗi chữ được phát bằng `lessonXX_id.wav`, sau đó chờ người dùng nhập Braille và nhấn Send.
- Sau khi âm thanh đã phát xong và firmware đang chờ đáp án, nhấn Study ngắn
  để nghe lại đúng âm thanh của chữ hiện tại. Thao tác này không đổi chữ và
  không thay đổi tiến độ.
- Nghe lại không phát lại các câu đúng, câu sai hoặc hint. Trong Practice, nhấn
  Practice để chỉ phát lại file `char_id.wav` của câu hỏi đang chờ.
- Đúng: phát câu đúng ngẫu nhiên và chuyển sang chữ tiếp theo.
- Sai: giữ nguyên chữ và cho nhập lại.
- Kết thúc bài: phát story và `study_next_or_repeat.wav`.
- Nhấn Study ngắn: phát `continue_next.wav` và sang bài tiếp theo.
- Giữ Study đủ 2 giây: phát `repeat_lesson.wav` và học lại từ đầu bài vừa xong.
- Giữ Study 2 giây trong lúc đang học cũng khởi động lại bài hiện tại.
- Bài 8 có 3 chữ theo báo cáo; các bài khác có 5 chữ.

### Practice

- Chỉ chọn ngẫu nhiên trong các chữ đã học và không chọn lại ngay câu trước.
- Sau khi câu hỏi đã phát xong và firmware đang chờ đáp án, nhấn Practice để
  nghe lại đúng chữ đang được hỏi. Firmware không chọn câu mới, không xóa số
  lần trả lời sai và không thay đổi đáp án đang chờ.
- Sai lần 1: `wrong_01_xx.wav`.
- Sai lần 2: `wrong_02_xx.wav` rồi `hint_xxx.wav`.
- Sai lần 3: `wrong_03_xx.wav` rồi `lessonXX_xxx.wav`.
- Sau lần sai thứ 3, giữ nguyên câu đến khi người dùng nhập đúng.
- Trong lúc phát âm thanh, vòng đọc nút bị chặn.

## CLI tiếng Nhật

Trong luồng câu hỏi thông thường, firmware chỉ in ký tự Nhật đang học hoặc đang được hỏi, ví dụ:

```text
あ
正解
い
```

Các thông báo trạng thái và lỗi cũng dùng tiếng Nhật. Lệnh kiểm tra vẫn dùng dạng ASCII để dễ gõ:

```text
study
hold_study
practice
replay
submit
mask 1
status
tone_ok
tone_fail
reset_progress
help
```

## Chạy Python CLI

```powershell
cd code_python
py -m pip install -r requirements.txt
py .\braille_cli.py --list-ports
py .\braille_cli.py --port COM9
```

Đóng Arduino Serial Monitor trước khi chạy Python; Windows chỉ cho một chương trình giữ cổng COM tại một thời điểm.

## Nạp firmware và âm thanh

1. Chuyển/kiểm tra âm thanh theo hướng dẫn trong `audio/README.md`.
2. Sync WAV vào `code_cpp/BrailleBuddy_JP/data/audio`.
3. Mở `code_cpp/BrailleBuddy_JP/BrailleBuddy_JP.ino` bằng Arduino IDE.
4. Chọn `ESP32S3 Dev Module`, `Flash Size: 16MB (128Mb)` và đúng cổng COM.
5. Upload firmware trước để ghi `partitions.csv` mới.
6. Nạp filesystem từ thư mục `data` bằng lệnh
   `Upload LittleFS to Pico/ESP8266/ESP32`, rồi reset bo mạch.

Kho âm thanh chưa được kèm vào project vì ba file DOCX chỉ cung cấp kịch bản và tên file, không chứa các bản WAV thực tế.

## Định dạng audio trong bản ADPCM

Firmware phát trực tiếp WAV IMA ADPCM được tạo bởi FFmpeg:

```text
Container: WAV
Codec: adpcm_ima_wav (format code 0x0011)
Sample rate: 12000 Hz
Channels: mono
Bit depth lưu trữ: 4 bit ADPCM
Đầu ra I2S: PCM signed 16 bit stereo
```

`AudioPlayer.h` đọc từng block từ LittleFS, dùng `ImaAdpcmDecoder.h` để
khôi phục PCM 16-bit, nhân cùng một mẫu sang hai kênh rồi gửi tới MAX98357A.
File không được nạp toàn bộ vào RAM.

`partitions.csv` dành 3 MiB cho firmware và `0xCE0000` byte (12,875 MiB)
cho LittleFS. Arduino IDE phải đặt `Flash Size: 16MB (128Mb)`.

Quy trình chuẩn hóa và đồng bộ:

```powershell
py .\code_python\prepare_audio.py convert .\audio_raw_backup --output .\audio --ignore-missing
py .\code_python\prepare_audio.py validate .\audio --ignore-missing
py .\code_python\prepare_audio.py sync .\audio --destination .\code_cpp\BrailleBuddy_JP\data\audio
```

Sau khi thêm hoặc đổi `partitions.csv`, upload firmware trước để ghi bảng
phân vùng, sau đó mới dùng lệnh `Upload LittleFS to Pico/ESP8266/ESP32`.

## Bản sửa lỗi phát âm thanh và chuyển chữ

- Sáu chấm Braille được chốt trong suốt thời gian giữ nút Send. Người dùng có
  thể thả các nút chấm gần thời điểm thả Send mà firmware vẫn giữ đúng mask.
- Sau đáp án đúng, Study chuyển trực tiếp sang phần tử kế tiếp trong cùng bài.
  Sau chữ cuối của bài, giữ Study đủ 2 giây để học lại; nếu không giữ, bài tiếp
  theo tự bắt đầu sau 3,5 giây.
- WAV sau giải mã được giới hạn ở 55% biên độ, thêm fade 8 ms và khoảng lặng
  ngắn giữa hai file để giảm clipping, tiếng rè và tiếng tách trên MAX98357A.
- Có thể chỉnh âm lượng lời nói bằng `AudioConfig::WAV_VOLUME_PERCENT` trong
  `BrailleConfig.h`. Nếu loa vẫn rè, thử `45`; nếu quá nhỏ và vẫn sạch, thử
  `60`. Không cần chuyển đổi hoặc nạp lại LittleFS khi chỉ đổi giá trị này.
