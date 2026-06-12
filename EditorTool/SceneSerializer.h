#pragma once

// 씬 직렬화 (.scene XML)
// - 대상: 에디터에서 생성 가능한 컴포넌트 셋
//   (Transform / MeshRenderer / ModelRenderer / ModelAnimator / Light / SkyCubeMap / ParticleSystem / Terrain)
// - 제외: IsEditorInternal() 오브젝트 (씬 카메라, 폴더 프리뷰 등)
// - 머티리얼: .mat 파일 기반이면 경로 참조(MaterialRef), 클론이면 MaterialDesc 인라인
class SceneSerializer
{
public:
	static bool Save(const wstring& path);
	static bool Load(const wstring& path);

	// 메모리 직렬화 — Undo/Redo 스냅샷, Play 스냅샷 등
	static bool SaveToString(string& out);
	static bool LoadFromString(const string& xml);

	// 오브젝트 복제 (자식 서브트리 포함, 원본과 같은 부모 밑) — 새 루트 id 반환 (-1 = 실패)
	static int64 Duplicate(int64 objectId);

	// 비-내부 오브젝트 전부 제거 (New Scene / Load 선행 단계)
	static void Clear();
};
