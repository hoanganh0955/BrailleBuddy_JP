# Thư mục âm thanh

Đặt các bản ghi gốc hoặc WAV đã chuẩn hóa vào đúng nhóm dưới đây. Firmware
sử dụng WAV IMA ADPCM mono, 12 kHz, khoảng 48 kbps. Khi phát, ESP32 giải mã
thành PCM 16-bit và gửi qua I2S tới MAX98357A.

## Cấu trúc

- `system/`: mở máy, chào quay lại, tổ hợp không tồn tại, kết thúc.
- `study/`: giới thiệu Study, chọn học tiếp/học lại.
- `lessons/`: tiêu đề, nội dung từng chữ và story của 10 bài.
- `chars/`: file đọc tên của 48 ký tự trong Practice.
- `hints/`: hint riêng của 48 ký tự.
- `practice/`: giới thiệu Practice, 5 câu mở đầu, câu đúng và ba cấp độ sai.

Tên file lấy nguyên từ ba báo cáo. Ví dụ:

- `lessons/lesson01_a.wav`
- `lessons/lesson01_story.wav`
- `chars/char_a.wav`
- `hints/hint_a.wav`
- `practice/correct_01.wav`
- `practice/wrong_02_01.wav`

## File mới cần ghi thêm

Các file sau không có sẵn trong ba báo cáo nhưng cần cho logic mới:

1. `system/welcome_back.wav`
   - Nhật: `おかえりなさい。続きから学ぶにはStudyボタンを、前のレッスンをもう一度学ぶにはPracticeボタンを押してください。`
   - Việt: Chào mừng bạn quay lại. Học tiếp nhấn Study, học lại bài cũ nhấn Practice.

2. `study/continue_next.wav`
   - Nhật: `次の文字から勉強を続けましょう。`
   - Việt: Chúng mình học tiếp từ chữ tiếp theo nhé.

3. `study/repeat_lesson.wav`
   - Nhật: `このレッスンを最初からもう一度学びましょう。`
   - Việt: Chúng mình học lại bài này từ đầu nhé.

4. `system/no_learned_items.wav`
   - Nhật: `まだ学習した文字がありません。先にStudyモードで学びましょう。`
   - Việt: Chưa có chữ nào đã học. Hãy vào Study trước nhé.

## Chuẩn hóa

Từ thư mục gốc của project:

```powershell
py .\code_python\prepare_audio.py convert C:\duong-dan\ban-ghi-goc --output .\audio
py .\code_python\prepare_audio.py validate .\audio --max-mib 12
py .\code_python\prepare_audio.py sync .\audio --destination .\code_cpp\BrailleBuddy_JP\data\audio
```

Nếu chưa ghi đủ toàn bộ catalog, thêm `--ignore-missing` vào lệnh `convert`
và `validate`. Script chỉ thay file đích sau khi FFmpeg chuyển đổi thành công
và `ffprobe` xác nhận `adpcm_ima_wav`, mono, 12 kHz. Vì vậy bản ghi gốc không
bị ghi đè nếu thư mục nguồn và thư mục đích khác nhau.

ESP32-S3-N16R8 có 16 MB flash. File `partitions.csv` trong sketch dành
12,875 MiB cho LittleFS; nên giữ tổng audio không quá 12 MiB để chừa metadata
filesystem. Không tăng `--max-mib` quá kích thước phân vùng thực tế.
