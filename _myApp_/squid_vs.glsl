#version 430 core

// -------------------------------------------------------
// 입력 : CPU(VBO)에서 넘어오는 정점 데이터
// -------------------------------------------------------
layout (location = 0) in vec3 pos;     // 정점 위치
layout (location = 1) in vec2 tex;     // UV 좌표
layout (location = 2) in vec3 normal;  // 법선 벡터

// -------------------------------------------------------
// 출력 : Fragment Shader로 전달할 보간 데이터
// -------------------------------------------------------
out vec3 vsPos;     // 월드 공간 위치
out vec3 vsNormal;  // 월드 공간 법선
out vec2 vsTex;     // UV 좌표

// -------------------------------------------------------
// 유니폼 : CPU에서 매 프레임 전달하는 행렬 및 플래그
// -------------------------------------------------------
uniform mat4 model;       // 모델 행렬 (로컬 → 월드)
uniform mat4 view;        // 뷰 행렬   (월드 → 카메라)
uniform mat4 projection;  // 투영 행렬 (카메라 → 클립)
uniform int  isModel;     // 1 = 3D 모델, 0 = 배경(박스)

void main() {
    // 정점을 월드 공간으로 변환
    vsPos = vec3(model * vec4(pos, 1.0));

    // 법선 벡터를 월드 공간으로 변환
    // - 배경(박스)은 안쪽 면을 보기 위해 법선을 반전(-normal)
    // - 모델은 그대로(normal) 사용
    mat3 normalMatrix = mat3(transpose(inverse(model)));
    vec3 faceNormal   = (isModel == 0) ? -normal : normal;
    vsNormal = normalize(normalMatrix * faceNormal);

    // UV 좌표는 그대로 전달
    vsTex = tex;

    // 최종 클립 공간 좌표 계산
    gl_Position = projection * view * vec4(vsPos, 1.0);
}
