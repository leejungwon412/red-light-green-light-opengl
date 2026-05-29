#include <sb7.h>
#include <vmath.h>
#include <shader.h>
#include <vector>
#include <iostream>
#include <cmath>
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "Model.h"

// SoLoud 사운드 엔진
#include "soloud.h"
#include "soloud_wav.h"

using namespace std;

// -------------------------------------------------------
// load_texture()
// 이미지 파일을 읽어 OpenGL 텍스처에 올림
// - 자동으로 RGB/RGBA 판별
// - 밉맵 생성, 반복 래핑, 선형 필터 적용
// -------------------------------------------------------
void load_texture(GLuint textureID, char const* filename) {
    glBindTexture(GL_TEXTURE_2D, textureID);

    int w, h, ch;
    stbi_set_flip_vertically_on_load(true); // OpenGL은 Y축이 반전되어 있어서 뒤집어줌
    unsigned char* data = stbi_load(filename, &w, &h, &ch, 0);

    if (data) {
        GLenum fmt = (ch == 4) ? GL_RGBA : GL_RGB; // 채널 수에 따라 포맷 결정
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    stbi_image_free(data);

    // 텍스처 래핑 및 필터 설정
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

// -------------------------------------------------------
// 열거형 : 인형(영희) 상태
// -------------------------------------------------------
enum DollState {
    GREEN_LIGHT, // 초록불 - 이동 가능
    RED_LIGHT    // 빨간불 - 멈춰야 함
};

// -------------------------------------------------------
// 열거형 : 게임 진행 상태
// -------------------------------------------------------
enum GameStatus {
    MENU,     // 메인 시작 화면
    PLAYING,  // 게임 진행 중
    GAMEOVER, // 사망 상태
    GAMEWIN   // 성공 상태 (결승선 통과)
};

// =======================================================
// RedLightGame 클래스
// sb7::application을 상속받아 게임 전체 로직을 담음
// =======================================================
class RedLightGame : public sb7::application {
public:
    // -------------------------------------------------------
    // 생성자 : 모든 멤버 변수 초기화
    // -------------------------------------------------------
    RedLightGame()
        : yaw(-90.0f), pitch(0.0f),
        lastX(400.0f), lastY(300.0f),
        firstMouse(true),
        deltaTime(0.0), lastFrame(0.0),
        gameStatus(MENU),
        lastGameStatus(MENU),
        currentState(GREEN_LIGHT),
        stateTimer(0.0f),
        dollRotationY(180.0f),
        hBgm(0), hVoice(0),
        leftButtonReleased(true),
        doll(nullptr), tree(nullptr)
    {
        // 키 입력 배열 초기화
        for (int i = 0; i < 1024; i++) keys[i] = false;

        // 카메라 초기 위치 및 방향
        cameraPos = vmath::vec3(0.0f, 1.5f, 70.0f);
        cameraFront = vmath::vec3(0.0f, 0.0f, -1.0f);
        cameraUp = vmath::vec3(0.0f, 1.0f, 0.0f);

        prevCameraPos = cameraPos;
    }

    // -------------------------------------------------------
    // 소멸자 : 사운드 엔진 정리
    // -------------------------------------------------------
    virtual ~RedLightGame() {
        soloudEngine.deinit();
    }

    // -------------------------------------------------------
    // startup()
    // 프로그램 시작 시 한 번 호출
    // - 셰이더 컴파일
    // - 배경 박스 VAO/VBO 설정
    // - 텍스처 로드
    // - 모델 로드
    // - 사운드 엔진 초기화
    // -------------------------------------------------------
    virtual void startup() {
        shader_program = compile_shader("squid_vs.glsl", "squid_fs.glsl");

        // --- 배경 박스 VAO/VBO 설정 ---
        glGenVertexArrays(1, VAOs);
        glGenBuffers(1, VBOs);
        glBindVertexArray(VAOs[0]);

        // 큐브 정점 데이터 (position 3 + normal 3 + uv 2 = 8 floats/vertex)
        // 각 면 2개의 삼각형 = 6 정점, 총 6면 = 36 정점
        GLfloat room[] = {
            // 뒷면 (z = -0.5)
            -0.5f,-0.5f,-0.5f, 0,0,-1, 0,0,
             0.5f,-0.5f,-0.5f, 0,0,-1, 1,0,
             0.5f, 0.5f,-0.5f, 0,0,-1, 1,1,
             0.5f, 0.5f,-0.5f, 0,0,-1, 1,1,
            -0.5f, 0.5f,-0.5f, 0,0,-1, 0,1,
            -0.5f,-0.5f,-0.5f, 0,0,-1, 0,0,
            // 앞면 (z = +0.5)
            -0.5f,-0.5f, 0.5f, 0,0, 1, 0,0,
             0.5f,-0.5f, 0.5f, 0,0, 1, 1,0,
             0.5f, 0.5f, 0.5f, 0,0, 1, 1,1,
             0.5f, 0.5f, 0.5f, 0,0, 1, 1,1,
            -0.5f, 0.5f, 0.5f, 0,0, 1, 0,1,
            -0.5f,-0.5f, 0.5f, 0,0, 1, 0,0,
            // 왼쪽 면 (x = -0.5)
            -0.5f, 0.5f, 0.5f,-1,0, 0, 1,1,
            -0.5f, 0.5f,-0.5f,-1,0, 0, 0,1,
            -0.5f,-0.5f,-0.5f,-1,0, 0, 0,0,
            -0.5f,-0.5f,-0.5f,-1,0, 0, 0,0,
            -0.5f,-0.5f, 0.5f,-1,0, 0, 1,0,
            -0.5f, 0.5f, 0.5f,-1,0, 0, 1,1,
            // 오른쪽 면 (x = +0.5)
             0.5f, 0.5f, 0.5f, 1,0, 0, 0,1,
             0.5f, 0.5f,-0.5f, 1,0, 0, 1,1,
             0.5f,-0.5f,-0.5f, 1,0, 0, 1,0,
             0.5f,-0.5f,-0.5f, 1,0, 0, 1,0,
             0.5f,-0.5f, 0.5f, 1,0, 0, 0,0,
             0.5f, 0.5f, 0.5f, 1,0, 0, 0,1,
             // 바닥 (y = -0.5)
             -0.5f,-0.5f,-0.5f, 0,-1,0, 0,1,
              0.5f,-0.5f,-0.5f, 0,-1,0, 1,1,
              0.5f,-0.5f, 0.5f, 0,-1,0, 1,0,
              0.5f,-0.5f, 0.5f, 0,-1,0, 1,0,
             -0.5f,-0.5f, 0.5f, 0,-1,0, 0,0,
             -0.5f,-0.5f,-0.5f, 0,-1,0, 0,1,
             // 천장 (y = +0.5)
             -0.5f, 0.5f,-0.5f, 0, 1,0, 0,1,
              0.5f, 0.5f,-0.5f, 0, 1,0, 1,1,
              0.5f, 0.5f, 0.5f, 0, 1,0, 1,0,
              0.5f, 0.5f, 0.5f, 0, 1,0, 1,0,
             -0.5f, 0.5f, 0.5f, 0, 1,0, 0,0,
             -0.5f, 0.5f,-0.5f, 0, 1,0, 0,1
        };

        glBindBuffer(GL_ARRAY_BUFFER, VBOs[0]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(room), room, GL_STATIC_DRAW);

        // location 0 : position (vec3)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
            8 * sizeof(float), 0);
        glEnableVertexAttribArray(0);

        // location 2 : normal (vec3)
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE,
            8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);

        // location 1 : uv (vec2)
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
            8 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // --- 텍스처 로드 ---
        glGenTextures(1, &wallTex); load_texture(wallTex, "squid_wall.jpg");
        glGenTextures(1, &sandTex); load_texture(sandTex, "sand.jpg");
        glGenTextures(1, &skyTex);  load_texture(skyTex, "squid_sky.jpg");
        glGenTextures(1, &dollTex); load_texture(dollTex, "doll.jpeg");
        glGenTextures(1, &treeTex); load_texture(treeTex, "tree.jpeg");

        // --- OBJ 모델 로드 ---
        doll = new Model("doll.obj");
        tree = new Model("tree.obj");

        // --- SoLoud 사운드 엔진 초기화 ---
        soloudEngine.init();

        bgmSound.load("main_bgm.mp3");
        bgmSound.setLooping(true);  // 메뉴 배경음 무한 반복

        voiceSound.load("voice.mp3");
        voiceSound.setLooping(false); // 영희 음성은 단발성

        // 시작 시 메뉴 BGM 재생
        hBgm = soloudEngine.play(bgmSound);

        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    // -------------------------------------------------------
    // updateGameLogic()
    // 매 프레임 게임 상태를 업데이트
    // - 메뉴 / 플레이 / 게임오버 상태별 처리
    // -------------------------------------------------------
    void updateGameLogic() {
        // 게임오버 or 클리어 → 메뉴로 복귀 시 BGM 재시작
        if (gameStatus == MENU && lastGameStatus != MENU) {
            soloudEngine.stopAll();
            hBgm = soloudEngine.play(bgmSound);
        }

        if (gameStatus == MENU) {
            // 메뉴에서는 영희가 정면(180도)을 천천히 바라봄
            dollRotationY += (180.0f - dollRotationY) * 5.0f * (float)deltaTime;
        }
        else if (gameStatus == PLAYING) {
            stateTimer += (float)deltaTime;

            // 초록불 4.15초 / 빨간불 5.0초
            // (4.15초는 "무궁화 꽃이 피었습니다" 음성과 싱크 맞춘 값)
            float stateDuration = (currentState == GREEN_LIGHT) ? 4.15f : 5.0f;

            if (stateTimer >= stateDuration) {
                stateTimer = 0.0f;

                if (currentState == GREEN_LIGHT) {
                    // 초록불 → 빨간불 전환
                    currentState = RED_LIGHT;
                }
                else {
                    // 빨간불 → 초록불 전환, 음성 새로 재생
                    currentState = GREEN_LIGHT;
                    soloudEngine.stop(hVoice);             // 이전 음성 정지
                    hVoice = soloudEngine.play(voiceSound); // 새 음성 시작
                }
            }

            // 영희가 바라볼 목표 각도 계산
            float targetAngle;
            if (currentState == GREEN_LIGHT) {
                // 초록불 : 영희가 반대쪽(180도)을 봄
                targetAngle = 180.0f;
            }
            else {
                // 빨간불 : 영희가 플레이어를 향해 돌아봄
                float dx = cameraPos[0] - 0.0f;
                float dz = cameraPos[2] - (-65.0f);
                targetAngle = atan2(dx, dz) * 180.0f / 3.1415926535f;
            }

            // 영희 회전 (빠르게 홱 돌아보는 효과)
            float rotationSpeed = 10.0f;
            dollRotationY += (targetAngle - dollRotationY) * rotationSpeed * (float)deltaTime;

            // --- 결승선 통과 판정 (Z <= -60) ---
            if (cameraPos[2] <= -60.0f) {
                gameStatus = GAMEWIN;
                soloudEngine.stopAll();
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }

            // --- 사망 판정 : 빨간불 + 영희가 플레이어를 보고 있을 때 움직이면 탈락 ---
            bool dollIsFacing = (abs(dollRotationY - targetAngle) < 15.0f);
            float dx = cameraPos[0] - prevCameraPos[0];
            float dz = cameraPos[2] - prevCameraPos[2];
            float moveDist = sqrt(dx * dx + dz * dz);

            if (currentState == RED_LIGHT && dollIsFacing && moveDist > 0.005f) {
                gameStatus = GAMEOVER;
                soloudEngine.stopAll();
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }

            prevCameraPos = cameraPos;
        }
        else if (gameStatus == GAMEOVER) {
            // 사망 시 카메라가 쓰러지듯 낮아짐 (Y가 0.2로 수렴)
            cameraPos[1] += (0.2f - cameraPos[1]) * 4.0f * (float)deltaTime;

            // 카메라 방향도 아래를 향하도록 pitch가 -35도로 수렴
            pitch += (-35.0f - pitch) * 3.0f * (float)deltaTime;

            cameraFront = vmath::normalize(vmath::vec3(
                cos(vmath::radians(yaw)) * cos(vmath::radians(pitch)),
                sin(vmath::radians(pitch)),
                sin(vmath::radians(yaw)) * cos(vmath::radians(pitch))
            ));
        }

        lastGameStatus = gameStatus;
    }

    // -------------------------------------------------------
    // render()
    // 매 프레임 화면을 그림
    // - 배경 렌더링
    // - 모델(인형, 나무) 렌더링
    // -------------------------------------------------------
    virtual void render(double currentTime) {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);

        // 델타타임 계산
        deltaTime = currentTime - lastFrame;
        lastFrame = currentTime;

        // --- 마우스 단발 클릭 감지 (Click Edge Detection) ---
        // 버튼을 누르고 떼었다가 다시 눌러야만 leftClicked = true
        bool leftPressed = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
        bool leftClicked = false;
        if (leftPressed) {
            if (leftButtonReleased) {
                leftClicked = true;
                leftButtonReleased = false;
            }
        }
        else {
            leftButtonReleased = true;
        }

        processInput();
        updateGameLogic();

        // --- 화면 초기화 ---
        const GLfloat black[] = { 0, 0, 0, 1 };
        glClearBufferfv(GL_COLOR, 0, black);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        float aspect = (float)width / (float)height;

        // --- 카메라 행렬 설정 ---
        vmath::mat4 proj = vmath::perspective(50.0f, aspect, 0.1f, 1000.0f);
        vmath::mat4 view = vmath::lookat(cameraPos, cameraPos + cameraFront, cameraUp);

        glUseProgram(shader_program);
        glUniformMatrix4fv(glGetUniformLocation(shader_program, "projection"), 1, GL_FALSE, proj);
        glUniformMatrix4fv(glGetUniformLocation(shader_program, "view"), 1, GL_FALSE, view);

        // --- 게임 상태를 셰이더에 전달 (0~4 인덱스) ---
        // 0: GreenLight, 1: RedLight, 2: GameOver, 3: GameWin, 4: Menu
        int statusIndex = 0;
        if (gameStatus == MENU)                        statusIndex = 4;
        else if (gameStatus == GAMEOVER)                    statusIndex = 2;
        else if (gameStatus == GAMEWIN)                     statusIndex = 3;
        else if (currentState == RED_LIGHT)                 statusIndex = 1;
        else                                                statusIndex = 0;

        // --- 마우스 호버 판정 (버튼 영역 체크) ---
        bool mouseHover = false;
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        // 스크린 좌표를 -0.5 ~ 0.5 중심 좌표로 변환 후 종횡비 보정
        float nx = (float)xpos / width;
        float ny = 1.0f - ((float)ypos / height);
        float px = (nx - 0.5f) * aspect;
        float py = ny - 0.5f;

        if (gameStatus == MENU) {
            // 메뉴 : 시작 버튼 범위 체크
            if (abs(px - 0.0f) <= 0.15f && abs(py - (-0.12f)) <= 0.05f) {
                mouseHover = true;

                if (leftClicked) {
                    // 게임 시작
                    gameStatus = PLAYING;
                    cameraPos = vmath::vec3(0.0f, 1.5f, 70.0f);
                    cameraFront = vmath::vec3(0.0f, 0.0f, -1.0f);
                    yaw = -90.0f;
                    pitch = 0.0f;
                    prevCameraPos = cameraPos;

                    currentState = GREEN_LIGHT;
                    stateTimer = 0.0f;
                    dollRotationY = 180.0f;

                    soloudEngine.stop(hBgm);
                    hVoice = soloudEngine.play(voiceSound); // 게임 시작과 함께 영희 음성 재생

                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                }
            }
        }
        else if (gameStatus == GAMEOVER || gameStatus == GAMEWIN) {
            // 게임오버 / 클리어 : 메뉴로 돌아가는 버튼 범위 체크
            if (abs(px - 0.0f) <= 0.15f && abs(py - (-0.12f)) <= 0.05f) {
                mouseHover = true;

                if (leftClicked) {
                    // 메뉴로 복귀
                    gameStatus = MENU;
                    cameraPos = vmath::vec3(0.0f, 1.5f, 70.0f);
                    cameraFront = vmath::vec3(0.0f, 0.0f, -1.0f);
                    yaw = -90.0f;
                    pitch = 0.0f;

                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                }
            }
        }

        // 셰이더에 HUD 관련 유니폼 전달
        glUniform1i(glGetUniformLocation(shader_program, "u_mouseHover"), mouseHover ? 1 : 0);
        glUniform1i(glGetUniformLocation(shader_program, "u_gameStatus"), statusIndex);
        glUniform2f(glGetUniformLocation(shader_program, "u_resolution"), (float)width, (float)height);

        // -------------------------------------------------------
        // [1] 배경 박스 렌더링
        // -------------------------------------------------------
        glUniform1i(glGetUniformLocation(shader_program, "isModel"), 0);

        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, wallTex);
        glUniform1i(glGetUniformLocation(shader_program, "wallTexture"), 0);

        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, sandTex);
        glUniform1i(glGetUniformLocation(shader_program, "floorTexture"), 1);

        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, skyTex);
        glUniform1i(glGetUniformLocation(shader_program, "skyTexture"), 2);

        // 배경 박스 : 가운데 위에 크게 배치
        vmath::mat4 mRoom = vmath::translate(0.0f, 20.0f, 0.0f)
            * vmath::scale(40.0f, 40.0f, 150.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader_program, "model"), 1, GL_FALSE, mRoom);
        glBindVertexArray(VAOs[0]);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // -------------------------------------------------------
        // [2] 3D 모델 렌더링 (인형, 나무)
        // -------------------------------------------------------
        glUniform1i(glGetUniformLocation(shader_program, "isModel"), 1);
        glDisable(GL_CULL_FACE); // OBJ 모델은 면 방향이 일정하지 않을 수 있어서 컬링 끔

        if (doll) {
            glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, dollTex);
            glUniform1i(glGetUniformLocation(shader_program, "modelTex"), 3);

            vmath::mat4 mDoll = vmath::translate(0.0f, 0.0f, -65.0f)
                * vmath::scale(13.0f)
                * vmath::rotate(dollRotationY, 0.0f, 1.0f, 0.0f)
                * doll->getCorrectionMatrix();
            glUniformMatrix4fv(glGetUniformLocation(shader_program, "model"), 1, GL_FALSE, mDoll);
            doll->draw();
        }

        if (tree) {
            glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, treeTex);
            glUniform1i(glGetUniformLocation(shader_program, "modelTex"), 3);

            vmath::mat4 mTree = vmath::translate(0.0f, 0.0f, -70.0f)
                * vmath::scale(25.0f)
                * vmath::rotate(0.0f, 1.0f, 0.0f, 0.0f)
                * tree->getCorrectionMatrix();
            glUniformMatrix4fv(glGetUniformLocation(shader_program, "model"), 1, GL_FALSE, mTree);
            tree->draw();
        }
    }

    // -------------------------------------------------------
    // processInput()
    // 키보드 입력으로 카메라 이동 처리
    // - PLAYING 상태에서만 동작
    // - 벽 바깥으로 나가지 못하게 클램핑
    // -------------------------------------------------------
    void processInput() {
        if (gameStatus == GAMEOVER || gameStatus == GAMEWIN) {
            // R키로 메뉴로 복귀
            if (keys['R']) {
                gameStatus = MENU;
                cameraPos = vmath::vec3(0.0f, 1.5f, 70.0f);
                cameraFront = vmath::vec3(0.0f, 0.0f, -1.0f);
                yaw = -90.0f;
                pitch = 0.0f;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            return;
        }

        if (gameStatus != PLAYING) return;

        float speed = 7.0f * (float)deltaTime;

        // yaw 각도 기준으로 전진/우측 방향 계산 (Y축 이동은 없음)
        float radYaw = vmath::radians(yaw);
        vmath::vec3 forward = vmath::vec3(cos(radYaw), 0.0f, sin(radYaw));
        vmath::vec3 right = vmath::vec3(-sin(radYaw), 0.0f, cos(radYaw));

        vmath::vec3 newPos = cameraPos;
        if (keys['W']) newPos += forward * speed;
        if (keys['S']) newPos -= forward * speed;
        if (keys['A']) newPos -= right * speed;
        if (keys['D']) newPos += right * speed;

        // 맵 범위 클램핑 (벽 안에서만 이동)
        if (newPos[0] < -19.5f) newPos[0] = -19.5f;
        if (newPos[0] > 19.5f) newPos[0] = 19.5f;
        if (newPos[2] < -74.5f) newPos[2] = -74.5f;
        if (newPos[2] > 74.5f) newPos[2] = 74.5f;

        cameraPos = newPos;
        cameraPos[1] = 1.5f; // 높이는 항상 1.5로 고정
    }

    // -------------------------------------------------------
    // onKey() : 키 입력 콜백 (sb7 프레임워크에서 호출)
    // -------------------------------------------------------
    void onKey(int k, int a) override {
        if (k >= 0 && k < 1024)
            keys[k] = (a != GLFW_RELEASE);
    }

    // -------------------------------------------------------
    // onMouseMove() : 마우스 이동 콜백
    // - yaw/pitch를 계산해 카메라 방향 갱신
    // -------------------------------------------------------
    void onMouseMove(int x, int y) override {
        if (gameStatus != PLAYING) return;

        // 첫 프레임 마우스 위치 초기화 (갑자기 카메라가 튀는 현상 방지)
        if (firstMouse) {
            lastX = (float)x;
            lastY = (float)y;
            firstMouse = false;
        }

        float offsetX = (float)x - lastX;
        float offsetY = lastY - (float)y; // Y는 위가 양수 방향
        lastX = (float)x;
        lastY = (float)y;

        float sensitivity = 0.15f;
        yaw += offsetX * sensitivity;
        pitch += offsetY * sensitivity;

        // pitch 범위 제한 (너무 위아래로 꺾이지 않게)
        if (pitch > 50.0f) pitch = 50.0f;
        if (pitch < -50.0f) pitch = -50.0f;

        // yaw/pitch로 카메라 방향 벡터 계산
        cameraFront = vmath::normalize(vmath::vec3(
            cos(vmath::radians(yaw)) * cos(vmath::radians(pitch)),
            sin(vmath::radians(pitch)),
            sin(vmath::radians(yaw)) * cos(vmath::radians(pitch))
        ));
    }

    // -------------------------------------------------------
    // compile_shader()
    // vs/fs 파일을 읽어 셰이더 프로그램을 컴파일·링크
    // -------------------------------------------------------
    GLuint compile_shader(const char* vs_file, const char* fs_file) {
        GLuint vs = sb7::shader::load(vs_file, GL_VERTEX_SHADER);
        GLuint fs = sb7::shader::load(fs_file, GL_FRAGMENT_SHADER);
        GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        return program;
    }

private:
    // --- 셰이더 및 버퍼 ---
    GLuint shader_program;
    GLuint VAOs[1], VBOs[1];

    // --- 텍스처 ---
    GLuint wallTex, sandTex, skyTex, dollTex, treeTex;

    // --- 3D 모델 ---
    Model* doll;
    Model* tree;

    // --- 카메라 ---
    vmath::vec3 cameraPos;
    vmath::vec3 cameraFront;
    vmath::vec3 cameraUp;
    float yaw, pitch;
    float lastX, lastY;
    bool  firstMouse;
    double deltaTime, lastFrame;

    // --- 입력 ---
    bool keys[1024];

    // --- 게임 상태 ---
    DollState  currentState;
    GameStatus gameStatus;
    GameStatus lastGameStatus;
    float      stateTimer;
    float      dollRotationY;
    vmath::vec3 prevCameraPos;

    // --- 마우스 클릭 감지 ---
    bool leftButtonReleased;

    // --- 사운드 ---
    SoLoud::Soloud soloudEngine;
    SoLoud::Wav    bgmSound;
    SoLoud::Wav    voiceSound;
    SoLoud::handle hBgm;
    SoLoud::handle hVoice;
};

DECLARE_MAIN(RedLightGame)