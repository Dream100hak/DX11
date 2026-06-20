// 공용 상수버퍼 (b0). C++ struct SceneCB(D3D12Device.h)와 바이트 단위 일치 필수.
cbuffer SceneCB : register(b0)
{
    row_major float4x4 gMVP;
    row_major float4x4 gModel;
    float4 gLightDir;   // xyz dir, w intensity
    float4 gCamPos;
    float4 gGridMin;
    float4 gGridMax;
    float4 gGridDim;    // x,y,z probe counts, w rays/probe
    float4 gGI;         // x giStrength, y frame, z ambient
    row_major float4x4 gInvVP; // 역 뷰프로젝션 (스카이 레이 복원)
    float4 gPointPos;   // xyz 점광원 위치, w 반경
    float4 gPointColor; // rgb 색, w 세기
    float4 gMatParams;  // x metallic, y roughness, z emissive, w albedoTint
    float4 gSunColor;   // rgb 태양색 (세기는 gLightDir.w)
    float4 gFog;        // rgb 안개색, w 밀도
    float4 gGrade;      // x 대비, y 채도, z 색온도, w 비네트
    float4 gSkyZenith;  // rgb 천정색, w 소프트섀도 반경
    float4 gSkyHorizon; // rgb 지평선색, w 태양 크기(지수)
    float4 gDebug;      // x 디버그뷰, y 프로브뷰, z 톤맵op, w 환경강도
    float4 gSpotPos;    // xyz 스팟 위치, w 반경
    float4 gSpotDir;    // xyz 스팟 방향, w cos(콘각)
    float4 gSpotColor;  // rgb 색×세기, w on
    float4 gTint;       // rgb 디퓨즈 틴트, w 바닥 거칠기
    float4 gPtPos[16];  // 다중 점광원 위치+반경 (MAX_PT)
    float4 gPtCol[16];  // 다중 점광원 색×세기 (w on)
    float4 gFloorMat;   // rgb 바닥 albedo, w 바닥 metallic
    float4 gAO;         // x on, y intensity, z radius
    float4 gShade;      // x toonLevels(0=off), y rimPower(0=off), z normalIntensity, w checker(0/1)
    float4 gRimColor;   // rgb 림 색
    float4 gGridParams; // x cell, y fade, z bgMode(0 sky/1 solid), w _
    float4 gOutline;    // rgb 색, w thickness
    float4 gDecal;      // xyz 데칼 위치, w 반경(0=off)
    float4 gDecalCol;   // rgb 데칼 색, w 구름량(0=off)
    float4 gExtra;      // x shadowStrength, y hemiAmbient, z stars(0/1), w _
    float4 gFog2;       // x 높이안개 시작Y, y 낙폭, z on(0/1), w _ — 높이 기반 안개
    float4 gDecalArr[8];    // xyz 위치, w 반경(0=off) — 다중 데칼(상향 투영)
    float4 gDecalColArr[8]; // rgb 색, w on
};
