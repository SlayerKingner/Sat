# Edu Saturator — ปลั๊กอิน saturation (VST3, macOS) สำหรับสอนนักเรียน

ปลั๊กอินที่มีฟังก์ชันเทียบเท่า Decapitator ตามคู่มือ v5 เขียนด้วย JUCE/C++ ตั้งใจให้อ่านโค้ดประกอบการสอนได้
(แบบจำลองเป็นของที่เขียนขึ้นเองตามลักษณะเสียงในคู่มือ ไม่ใช่อัลกอริทึมของ Soundtoys)

## วิธีสร้างไฟล์ .vst3 บน Mac (ต้องมี Xcode และ CMake)
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
ไฟล์จะถูกคัดลอกไปที่ `~/Library/Audio/Plug-Ins/VST3/Edu Saturator.vst3` อัตโนมัติ (ปิดได้ด้วย `-DEDU_COPY_AFTER_BUILD=OFF`)
ครั้งแรกใช้เวลาหลายนาทีเพราะดาวน์โหลด JUCE 8.0.4

## ไม่มี Mac / ไม่อยากลง Xcode
อัปโหลดโฟลเดอร์นี้ขึ้น GitHub แล้วไปที่ Actions > "Build VST3 (macOS)" > Run workflow
จะได้ไฟล์ zip ของ .vst3 ให้ดาวน์โหลด (Universal: Apple Silicon + Intel)

## ถ้า DAW ไม่เห็นปลั๊กอินหรือ macOS บล็อก
ไฟล์ที่ดาวน์โหลดมาจะถูกติด quarantine ให้รัน
`xattr -cr "/Library/Audio/Plug-Ins/VST3/Edu Saturator.vst3"` (หรือพาธที่วางไว้) แล้ว Rescan ใน DAW
ปลั๊กอินไม่ได้ sign ด้วย Developer ID ซึ่งปกติสำหรับการใช้ในห้องเรียน

## ลำดับสัญญาณ (Signal flow)
Low Cut (HPF 12 dB/oct) → Thump → Tone (tilt EQ ±6 dB) → สีของ Style (EQ) → Drive (0–10 = 0–30 dB) + Punish (+20 dB)
→ **Saturation @4x oversampling** + DC blocker → High Cut (6 หรือ 30 dB/oct) → สีหลัง saturation → Output → Mix (dry/wet)

| Style | จำลองลักษณะของ | เส้นโค้งใน `saturate()` | ฮาร์โมนิกเด่น |
|---|---|---|---|
| A | พรีแอมป์เทปหลอด | tanh นุ่ม + ตัดแหลมเล็กน้อย | คี่ นุ่ม |
| E | คอนโซล EMI TG | soft-knee อสมมาตรเล็กน้อย + เบสหนา แหลมโปร่ง | คู่+คี่ |
| N | ทรานซิสเตอร์เจอร์เมเนียม | ครึ่งบวก/ลบ clip ไม่เท่ากัน + มิดโฟกัส | คู่+คี่ |
| T | หลอดไตรโอด | tanh มี bias สูง | **คู่** |
| P | หลอดเพนโทด | x / ³√(1+\|x\|³) สมมาตร | **คี่** |

- Attitude Meter วัด RMS หลัง Drive (ไม่ใช่เอาต์พุต) 0 VU ≈ -12 dBFS
- Auto: Output = −(Drive รวม × 0.5) dB ปุ่ม Output จะขยับสวนทาง Drive
- Latency ของ oversampling ถูกรายงานให้ DAW และ dry ถูกหน่วงให้ตรงกับ wet

## ทดสอบ DSP (ไม่บังคับ)
`cmake -S . -B build-test -DEDU_BUILD_TESTS=ON && cmake --build build-test --target EduDspTest`
แล้วรันไฟล์ EduDspTest ตรวจ: ฮาร์โมนิกของแต่ละ Style, Punish, High Cut/Steep, Auto, Mix

## ไฟล์สำคัญ
`Source/PluginProcessor.cpp` (DSP ทั้งหมด), `Source/PluginEditor.cpp` (GUI), `Tests/DspTest.cpp`
