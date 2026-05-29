#version 430 core

// -------------------------------------------------------
// 입력 : Vertex Shader에서 보간되어 넘어오는 데이터
// -------------------------------------------------------
in vec3 vsPos;     // 월드 공간 위치
in vec3 vsNormal;  // 월드 공간 법선
in vec2 vsTex;     // UV 좌표

// -------------------------------------------------------
// 출력 : 최종 픽셀 색상
// -------------------------------------------------------
out vec4 color;

// -------------------------------------------------------
// 텍스처 유니폼
// -------------------------------------------------------
uniform sampler2D wallTexture;   // 벽 텍스처
uniform sampler2D floorTexture;  // 바닥 텍스처
uniform sampler2D skyTexture;    // 천장 텍스처
uniform sampler2D modelTex;      // 모델 텍스처 (인형, 나무 등)

// -------------------------------------------------------
// 게임 상태 유니폼
// -------------------------------------------------------
uniform int   isModel;        // 1 = 3D 모델, 0 = 배경 박스
uniform vec2  u_resolution;   // 화면 해상도 (픽셀 단위)
uniform int   u_gameStatus;   // 0: GreenLight, 1: RedLight, 2: GameOver, 3: GameWin, 4: Menu
uniform int   u_mouseHover;   // 1: 버튼 위에 마우스 있음, 0: 없음

// =======================================================
// SDF(Signed Distance Field) 도형 함수들
// 각 함수는 점 p에서 도형 표면까지의 거리를 반환
// 거리가 0보다 작으면 도형 내부, 크면 외부
// =======================================================

// 원 SDF
float sdCircle(vec2 p, float r) {
    return length(p) - r;
}

// 직사각형 SDF (b = 반너비, 반높이)
float sdBox(vec2 p, vec2 b) {
    vec2 d = abs(p) - b;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

// 정삼각형 SDF (r = 크기)
float sdEquilateralTriangle(vec2 p, float r) {
    const float k = sqrt(3.0);
    p.x = abs(p.x) - r;
    p.y = p.y + r / k;
    if (p.x + k * p.y > 0.0)
        p = vec2(p.x - k * p.y, -k * p.x - p.y) / 2.0;
    p.x -= clamp(p.x, -2.0 * r, 0.0);
    return -length(p) * sign(p.y);
}

// 집 모양 SDF (삼각 지붕 + 직사각형 몸체 + 문 구멍)
float sdHouse(vec2 p) {
    float roof = sdEquilateralTriangle(p - vec2(0.0,  0.004), 0.012);
    float body = sdBox(p - vec2(0.0, -0.006), vec2(0.009, 0.006));
    float door = sdBox(p - vec2(0.0, -0.009), vec2(0.003, 0.004));

    float house = min(roof, body); // 지붕과 몸체를 합침
    return max(house, -door);      // 문 구멍을 뚫음 (Boolean Subtraction)
}

// =======================================================
// main()
// =======================================================
void main() {
    vec3 norm = normalize(vsNormal);

    // -------------------------------------------------------
    // [1단계] 배경 or 모델 기본 색상 계산
    // -------------------------------------------------------
    if (isModel == 0) {
        // --- 배경 박스 : 법선 방향으로 바닥/천장/벽 구분 ---
        if (norm.y > 0.4) {
            // 바닥 : UV를 늘려서 타일처럼 보이게
            vec2 uv  = vsTex;
            uv.x    *= 4.0;
            uv.y    *= 15.0;
            color    = texture(floorTexture, uv);

            // 결승선 : Z = -60 부근을 핑크색으로
            if (abs(vsPos.z - (-60.0)) < 0.3)
                color = vec4(0.9, 0.2, 0.4, 1.0);
        }
        else if (norm.y < -0.4) {
            // 천장 : UV를 Y방향으로 늘림
            vec2 uv = vsTex;
            uv.y   *= 4.0;
            color   = texture(skyTexture, uv);
        }
        else {
            // 벽면 : 옆면은 X방향으로 UV를 늘림
            vec2 uv = vsTex;
            if (abs(norm.x) > 0.5)
                uv.x *= 4.0;
            color = texture(wallTexture, uv);

            // 앞뒤 벽은 살짝 어둡게 (음영 효과)
            color.rgb *= (0.7 + 0.3 * abs(norm.z));
        }
    }
    else {
        // --- 3D 모델 : 텍스처 + 간단한 디퓨즈 조명 ---
        vec4  tCol      = texture(modelTex, vsTex);
        vec3  lightDir  = normalize(vec3(0.3, 1.0, 0.5));
        float diffuse   = max(dot(norm, lightDir), 0.5); // 최소 0.5로 너무 어둡지 않게
        color = vec4(tCol.rgb * diffuse, 1.0);
    }

    // -------------------------------------------------------
    // [2단계] 비네팅 효과 (화면 가장자리를 어둡게)
    // -------------------------------------------------------
    vec2  screenUV = gl_FragCoord.xy / u_resolution;
    float vignette = screenUV.x * screenUV.y
                   * (1.0 - screenUV.x) * (1.0 - screenUV.y);
    vignette = clamp(pow(16.0 * vignette, 0.25), 0.0, 1.0);
    color.rgb *= mix(0.95, 1.0, vignette);

    // -------------------------------------------------------
    // [3단계] 화면 좌표 기준 HUD 계산용 변수 준비
    // -------------------------------------------------------
    float aspect    = u_resolution.x / u_resolution.y;
    vec2  p         = screenUV - vec2(0.5); // 화면 중앙이 (0,0)
    p.x            *= aspect;               // 종횡비 보정

    // 신호등 위치 (화면 상단 중앙)
    vec2  lightCenter = vec2(0.0, 0.40);
    float distToLight = length(p - lightCenter);

    // -------------------------------------------------------
    // [4단계] 게임 상태별 HUD 오버레이
    // -------------------------------------------------------
    if (u_gameStatus == 4) {
        // ==================== 메인 메뉴 화면 ====================
        color.rgb *= 0.25; // 배경을 어둡게

        // 오징어 게임 로고 : 원, 삼각형, 사각형 테두리
        float d_circle   = sdCircle(p - vec2(-0.22, 0.15), 0.08);
        float d_triangle = sdEquilateralTriangle(p - vec2(0.0,  0.15), 0.08);
        float d_square   = sdBox(p - vec2(0.22,  0.15), vec2(0.07));

        float thickness = 0.007;
        float logo_c    = smoothstep(0.002, 0.0, abs(d_circle)   - thickness);
        float logo_t    = smoothstep(0.002, 0.0, abs(d_triangle) - thickness);
        float logo_s    = smoothstep(0.002, 0.0, abs(d_square)   - thickness);
        float logoMask  = max(max(logo_c, logo_t), logo_s);

        color.rgb = mix(color.rgb, vec3(1.0, 0.1, 0.4), logoMask);

        // 시작 버튼 (화면 중앙 하단)
        vec2  btnCenter = vec2(0.0, -0.12);
        float d_btn     = sdBox(p - btnCenter, vec2(0.15, 0.05));
        float btnOutline = smoothstep(0.002, 0.0, abs(d_btn) - 0.002);
        float btnFill    = smoothstep(0.0, -0.002, d_btn);

        // 플레이 삼각형 아이콘 (버튼 중앙)
        vec2  p_play     = p - btnCenter;
        vec2  p_play_rot = vec2(-p_play.y, p_play.x); // 90도 회전 (삼각형이 오른쪽을 향하게)
        float d_play     = sdEquilateralTriangle(p_play_rot, 0.015);
        float playMask   = smoothstep(0.0, -0.002, d_play);

        if (u_mouseHover == 1) {
            // 호버 상태 : 핑크 강조
            color.rgb = mix(color.rgb, vec3(1.0, 0.1,  0.4),  btnFill * 0.35);
            color.rgb = mix(color.rgb, vec3(1.0, 0.25, 0.55), btnOutline);
            color.rgb = mix(color.rgb, vec3(1.0, 1.0,  1.0),  playMask);
        } else {
            // 기본 상태 : 어두운 배경
            color.rgb = mix(color.rgb, vec3(0.08, 0.08, 0.08), btnFill);
            color.rgb = mix(color.rgb, vec3(0.65, 0.08, 0.25), btnOutline);
            color.rgb = mix(color.rgb, vec3(0.65, 0.08, 0.25), playMask);
        }
    }
    else {
        // ==================== 인게임 및 결과 화면 ====================

        if (u_gameStatus == 0) {
            // --- 초록불 : 신호등을 초록색으로 ---
            if      (distToLight < 0.020) color.rgb = vec3(0.0, 1.0, 0.2);  // 초록 불빛
            else if (distToLight < 0.022) color.rgb = vec3(0.1, 0.1, 0.1);  // 테두리
        }
        else if (u_gameStatus == 1) {
            // --- 빨간불 : 신호등을 빨간색으로 ---
            if      (distToLight < 0.020) color.rgb = vec3(1.0, 0.0, 0.1);  // 빨간 불빛
            else if (distToLight < 0.022) color.rgb = vec3(0.1, 0.1, 0.1);  // 테두리
        }
        else if (u_gameStatus == 2) {
            // --- 게임 오버 : 빨간 주변부 비네팅 + 재시작 버튼 ---
            float distFromCenter  = length(screenUV - vec2(0.5, 0.5));
            float redDangerBorder = smoothstep(0.25, 0.85, distFromCenter);

            // 화면 전체를 붉게 물들임
            vec3 deadBase = mix(color.rgb, vec3(0.25, 0.05, 0.05), 0.25);
            color.rgb = mix(deadBase, vec3(0.70, 0.15, 0.15), redDangerBorder * 0.45);

            // 재시작 버튼
            vec2  btnCenter  = vec2(0.0, -0.12);
            float d_btn      = sdBox(p - btnCenter, vec2(0.15, 0.05));
            float btnOutline = smoothstep(0.002, 0.0, abs(d_btn) - 0.002);
            float btnFill    = smoothstep(0.0, -0.002, d_btn);

            // 리트라이 아이콘 : 원호(arc) + 화살표
            vec2  p_retry = p - btnCenter;
            float d_arc   = abs(length(p_retry) - 0.015) - 0.003;
            if (p_retry.x > 0.0 && p_retry.y > 0.0)
                d_arc = 1e9; // 1사분면 제거 → 화살표 자리 확보

            float d_arrow     = sdEquilateralTriangle(vec2(p_retry.y - 0.004, p_retry.x - 0.015), 0.005);
            float retryMask   = max(smoothstep(0.001, 0.0, d_arc), smoothstep(0.002, 0.0, d_arrow));

            if (u_mouseHover == 1) {
                // 호버 상태 : 밝은 빨강 강조
                color.rgb = mix(color.rgb, vec3(0.9,  0.15, 0.15), btnFill * 0.3);
                color.rgb = mix(color.rgb, vec3(1.0,  0.35, 0.35), btnOutline);
                color.rgb = mix(color.rgb, vec3(1.0,  1.0,  1.0),  retryMask);
            } else {
                // 기본 상태 : 어두운 빨강
                color.rgb = mix(color.rgb, vec3(0.06, 0.04, 0.04), btnFill);
                color.rgb = mix(color.rgb, vec3(0.65, 0.15, 0.20), btnOutline);
                color.rgb = mix(color.rgb, vec3(0.65, 0.15, 0.20), retryMask);
            }
        }
        else if (u_gameStatus == 3) {
            // --- 게임 클리어 : 황금빛 오버레이 + 로고 + 홈 버튼 ---
            color.rgb = mix(color.rgb, vec3(1.0, 0.88, 0.60), 0.20); // 황금빛 필터

            // 오징어 게임 로고 (초록색)
            float d_circle   = sdCircle(p - vec2(-0.22, 0.15), 0.08);
            float d_triangle = sdEquilateralTriangle(p - vec2(0.0,  0.15), 0.08);
            float d_square   = sdBox(p - vec2(0.22,  0.15), vec2(0.07));

            float thickness = 0.007;
            float logo_c    = smoothstep(0.002, 0.0, abs(d_circle)   - thickness);
            float logo_t    = smoothstep(0.002, 0.0, abs(d_triangle) - thickness);
            float logo_s    = smoothstep(0.002, 0.0, abs(d_square)   - thickness);
            float logoMask  = max(max(logo_c, logo_t), logo_s);

            color.rgb = mix(color.rgb, vec3(0.0, 1.0, 0.2), logoMask);

            // 홈 버튼
            vec2  btnCenter  = vec2(0.0, -0.12);
            float d_btn      = sdBox(p - btnCenter, vec2(0.15, 0.05));
            float btnOutline = smoothstep(0.002, 0.0, abs(d_btn) - 0.002);
            float btnFill    = smoothstep(0.0, -0.002, d_btn);

            // 집 아이콘
            vec2  p_home  = p - btnCenter;
            float d_house = sdHouse(p_home);
            float homeMask = smoothstep(0.0, -0.002, d_house);

            if (u_mouseHover == 1) {
                // 호버 상태 : 밝은 초록 강조
                color.rgb = mix(color.rgb, vec3(0.15, 0.80, 0.30), btnFill * 0.3);
                color.rgb = mix(color.rgb, vec3(0.35, 1.00, 0.50), btnOutline);
                color.rgb = mix(color.rgb, vec3(1.0,  1.0,  1.0),  homeMask);
            } else {
                // 기본 상태 : 어두운 초록
                color.rgb = mix(color.rgb, vec3(0.04, 0.06, 0.04), btnFill);
                color.rgb = mix(color.rgb, vec3(0.15, 0.65, 0.20), btnOutline);
                color.rgb = mix(color.rgb, vec3(0.15, 0.65, 0.20), homeMask);
            }
        }
    }
}
