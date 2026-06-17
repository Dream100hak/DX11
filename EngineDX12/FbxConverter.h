#pragma once
#include <string>

// ───────────────────────────────────────────────────────────
// FBX → .mesh/.clip/.mat/.mmat 변환기 (ufbx 기반)
//   DX11 EditorTool/UfbxConverter 를 EngineDX12 로 자체완결 이식.
//   출력 바이너리 포맷은 MeshLoader.h(=DX11 FileUtils 직렬화)와 바이트 호환:
//     string = uint32 길이 + 원시 바이트(널 없음)
//   - 메시: <outDir>/<stem>.mesh
//   - 머티리얼: <outDir>/<stem>.mmat + 머티리얼별 .mat (+ 텍스처 추출)
//   - 애니메이션: <outDir>/<stem>.clip (anim_stack[0])
// 좌표계: 좌수 Y-up + Z 미러, target_unit_meters=1, MODIFY_GEOMETRY (DX11 산출물과 스케일 일치)
// ───────────────────────────────────────────────────────────
struct FbxConvertResult
{
	bool   ok = false;
	int    boneCount = 0;
	int    meshCount = 0;
	int    materialCount = 0;
	int    animCount = 0;
	int    frameCount = 0;
	std::string error;       // ok==false 시 사유
	std::wstring meshPath;   // 생성된 .mesh 절대경로
	std::wstring clipPath;   // 생성된 .clip (없으면 빈 문자열)
};

// fbxPath: 변환할 FBX 절대경로 / outDir: 출력 폴더(끝에 \ 포함 권장) / stem: 파일 스템(확장자 제외)
FbxConvertResult ConvertFbxToMesh(const std::wstring& fbxPath, const std::wstring& outDir, const std::wstring& stem);
