#ifndef MODEL_H
#define MODEL_H

#include <sb7.h>
#include <vmath.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

// -------------------------------------------------------
// Vertex 구조체
// 하나의 꼭짓점(vertex)이 가지는 데이터 묶음
// - Position  : 3D 좌표 (x, y, z)
// - TexCoords : 텍스처 UV 좌표 (u, v)
// - Normal    : 법선 벡터 (빛 계산에 사용)
// -------------------------------------------------------
struct Vertex {
    vmath::vec3 Position;
    vmath::vec2 TexCoords;
    vmath::vec3 Normal;
};

// -------------------------------------------------------
// Model 클래스
// OBJ 파일을 읽어 GPU에 올리고, 그리는 역할
// -------------------------------------------------------
class Model {
public:
    GLuint VAO, VBO;   // GPU 버퍼 핸들
    int vertexCount;   // 총 정점 개수
    vmath::vec3 minB;  // 바운딩 박스 최솟값
    vmath::vec3 maxB;  // 바운딩 박스 최댓값

    // 생성자: OBJ 파일 경로를 받아 바로 로드
    Model(const char* path) : vertexCount(0) {
        minB = vmath::vec3(1e9f);
        maxB = vmath::vec3(-1e9f);
        loadOBJ(path);
    }

    // --------------------------------------------------
    // getCorrectionMatrix()
    // 모델을 화면 중앙에 맞게 정규화하는 행렬 반환
    // - 모델의 중심을 원점으로 이동
    // - 가장 큰 축 기준으로 크기를 1로 맞춤
    // --------------------------------------------------
    vmath::mat4 getCorrectionMatrix() {
        vmath::vec3 center = (minB + maxB) * 0.5f;
        vmath::vec3 size = maxB - minB;

        float maxDim = std::max({ size[0], size[1], size[2] });
        if (maxDim < 0.001f) maxDim = 1.0f; // 너무 작은 모델 예외 처리

        // 크기 정규화 후, 바닥이 Y=0에 닿도록 이동
        return vmath::scale(1.0f / maxDim)
            * vmath::translate(-center[0], -minB[1], -center[2]);
    }

    // --------------------------------------------------
    // draw()
    // VAO를 바인딩하고 삼각형을 그림
    // --------------------------------------------------
    void draw() {
        if (vertexCount == 0) return;

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
        glBindVertexArray(0);
    }

private:
    // --------------------------------------------------
    // loadOBJ()
    // OBJ 파일을 파싱해서 GPU 버퍼(VAO/VBO)에 올림
    // --------------------------------------------------
    void loadOBJ(const char* path) {
        // OBJ 파일에서 읽어온 임시 데이터
        std::vector<vmath::vec3> temp_vertices;
        std::vector<vmath::vec2> temp_uvs;
        std::vector<vmath::vec3> temp_normals;

        // 최종적으로 GPU에 올릴 정점 리스트
        std::vector<Vertex> out_vertices;

        std::ifstream file(path);
        if (!file.is_open()) return; // 파일 열기 실패 시 종료

        std::string line;
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string prefix;
            ss >> prefix;

            if (prefix == "v") {
                // 정점 좌표 파싱
                vmath::vec3 v;
                ss >> v[0] >> v[1] >> v[2];
                temp_vertices.push_back(v);

                // 바운딩 박스 갱신
                for (int i = 0; i < 3; i++) {
                    minB[i] = std::min(minB[i], v[i]);
                    maxB[i] = std::max(maxB[i], v[i]);
                }
            }
            else if (prefix == "vt") {
                // UV 좌표 파싱
                vmath::vec2 vt;
                ss >> vt[0] >> vt[1];
                temp_uvs.push_back(vt);
            }
            else if (prefix == "vn") {
                // 법선 벡터 파싱
                vmath::vec3 vn;
                ss >> vn[0] >> vn[1] >> vn[2];
                temp_normals.push_back(vn);
            }
            else if (prefix == "f") {
                // 면(face) 파싱: 삼각형 3개 정점 처리
                for (int i = 0; i < 3; i++) {
                    std::string vData;
                    ss >> vData;

                    // "v/vt/vn" 형식의 슬래시를 공백으로 교체 후 파싱
                    std::replace(vData.begin(), vData.end(), '/', ' ');
                    std::stringstream vss(vData);

                    int vi = 0, ti = 0, ni = 0;
                    vss >> vi >> ti >> ni;

                    Vertex v;

                    // 인덱스가 1 이상일 때만 대입 (OBJ 인덱스는 1부터 시작)
                    if (vi > 0) v.Position = temp_vertices[vi - 1];
                    if (ti > 0) v.TexCoords = temp_uvs[ti - 1];
                    else        v.TexCoords = vmath::vec2(0, 0);
                    if (ni > 0) v.Normal = temp_normals[ni - 1];
                    else        v.Normal = vmath::vec3(0, 1, 0);

                    out_vertices.push_back(v);
                }
            }
        }

        vertexCount = (int)out_vertices.size();
        if (vertexCount == 0) return;

        // VAO, VBO 생성 및 바인딩
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);

        // 정점 데이터를 GPU에 업로드
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER,
            out_vertices.size() * sizeof(Vertex),
            &out_vertices[0],
            GL_STATIC_DRAW);

        // location 0 : Position (vec3)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
            sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(0);

        // location 1 : TexCoords (vec2)
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
            sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));
        glEnableVertexAttribArray(1);

        // location 2 : Normal (vec3)
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE,
            sizeof(Vertex), (void*)offsetof(Vertex, Normal));
        glEnableVertexAttribArray(2);
    }
};

#endif