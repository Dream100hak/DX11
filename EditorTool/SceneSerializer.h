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
};
