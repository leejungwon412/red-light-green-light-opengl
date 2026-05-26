# 🔴 Red Light, Green Light — OpenGL 3D Game

> **무궁화꽃이피었습니다** — 오징어게임에서 영감을 받은 1인칭 3D 서바이벌 게임  
> OpenGL(sb7 프레임워크) 기반으로 제작된 프로젝트

---

## 📸 Overview

플레이어는 1인칭 시점으로 필드를 가로질러 결승선을 통과하는 것이 목표입니다.  
영희 인형이 뒤를 돌아보는 **빨간불(Red Light)** 상태에서 움직이면 즉시 게임오버됩니다.

---

## 🎮 Gameplay

| 상태 | 설명 |
|------|------|
| 🟢 Green Light | 자유롭게 이동 가능 |
| 🔴 Red Light | 정지! 움직이면 즉시 사망 |
| 💀 Game Over | 빨간불에 움직임이 감지됨 |
| 🎉 Game Win | 결승선(z = -60) 통과 성공 |

### 조작 방법

| 키 / 입력 | 동작 |
|-----------|------|
| `W` `A` `S` `D` | 앞/왼/뒤/오른쪽 이동 |
| 마우스 이동 | 시점 회전 |
| 화면 버튼 클릭 | 게임 시작 / 재시작 |
| `R` | 게임오버/클리어 후 메인으로 복귀 |

---

## 🛠️ 기술 스택

- **언어**: C++
- **그래픽스 API**: OpenGL 4.3
- **프레임워크**: [sb7](https://github.com/openglsuperbible/sb7code) (OpenGL SuperBible 7)
- **수학 라이브러리**: vmath (sb7 내장)
- **윈도우 / 입력**: GLFW
- **텍스처 로딩**: stb_image
- **오디오 엔진**: [SoLoud](https://solhsa.com/soloud/)
- **개발 환경**: Visual Studio 2026

---

## ✨ 주요 구현 사항

### 렌더링
- 커스텀 OBJ 로더로 3D 모델(영희 인형, 나무) 로드 및 렌더링
- 바닥 / 벽 / 천장에 각각 다른 텍스처 적용 (UV 반복 타일링)
- Diffuse lighting 적용 (모델 음영 처리)
- 비네팅(Vignette) 포스트 프로세싱 효과

### 셰이더 (GLSL)
- **Vertex Shader**: MVP 변환, 법선 변환
- **Fragment Shader**:  
  - **SDF(Signed Distance Field)** 기반 UI 렌더링 — 버튼, 아이콘을 텍스처 없이 수식으로 직접 구현
  - 메인 메뉴: 오징어게임 로고(○△□) SDF 렌더링
  - 인게임: 상단 초록불 / 빨간불 인디케이터
  - 게임오버: 붉은 화면 비네팅 오버레이
  - 클리어: 골든 톤 + 초록 로고 오버레이

### 게임 로직
- 빨간불 전환 시 영희 인형이 플레이어 방향으로 회전 (실시간 atan2 계산)
- 빨간불 상태에서 이동 거리(moveDist) 측정으로 사망 판정
- 음성("무궁화꽃이피었습니다") 재생 타이밍과 영희 회전 싱크 맞춤

### 오디오
- 메인 BGM 무한 루프
- 게임 시작 시 BGM 정지 및 "무궁화꽃이피었습니다" 음성 반복 재생
- 사망 / 클리어 / 메인 복귀 시 오디오 상태 자동 전환

---

## 📁 파일 구조

```
├── final.cpp          # 메인 애플리케이션 (게임 로직, 렌더링)
├── model.h            # OBJ 3D 모델 로더
├── squid_vs.glsl      # Vertex Shader
├── squid_fs.glsl      # Fragment Shader (UI, 조명, 포스트 프로세싱)
├── stb_image.h        # 텍스처 로딩 라이브러리
├── doll.obj           # 영희 인형 3D 모델
├── tree.obj           # 나무 3D 모델
├── doll.jpeg          # 영희 텍스처
├── tree.jpeg          # 나무 텍스처
├── squid_wall.jpg     # 벽면 텍스처
├── sand.jpg           # 바닥 텍스처
├── squid_sky.jpg      # 천장 텍스처
├── main_bgm.mp3       # 메인 화면 BGM
└── voice.mp3          # "무궁화꽃이피었습니다" 음성
```

---

## 🚀 빌드 및 실행

> Visual Studio 2026 기준

1. sb7, GLFW, SoLoud 라이브러리가 프로젝트에 포함되어 있어야 합니다.
2. 솔루션 파일을 열고 **빌드(Ctrl+Shift+B)** 후 실행합니다.
3. 실행 파일과 같은 디렉토리에 텍스처, 모델, 오디오 파일이 있어야 합니다.

---

## 📝 License

본 프로젝트는 학습 목적으로 제작된 개인 포트폴리오입니다.
