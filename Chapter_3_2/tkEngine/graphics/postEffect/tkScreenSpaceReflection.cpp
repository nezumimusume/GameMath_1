/*!
 *@brief	Screen space reflection
 */
#include "tkEngine/tkEnginePreCompile.h"
#include "tkEngine/graphics/postEffect/tkScreenSpaceReflection.h"
#include "tkEngine/graphics/tkPresetRenderState.h"
#include "tkEngine/graphics/tkPresetSamplerState.h"

namespace tkEngine {
	void CScreenSpaceReflection::Release()
	{
	}
	void CScreenSpaceReflection::Init(const SGraphicsConfig& config)
	{
		m_isEnable = config.ssrConfig.isEnable;
		m_vsShader.Load("shader/ScreenSpaceReflection.fx", "VSMain", CShader::EnType::VS);
		m_psShader.Load("shader/ScreenSpaceReflection.fx", "PSMain", CShader::EnType::PS);
		m_psFinalShader.Load("shader/ScreenSpaceReflection.fx", "PSFinal", CShader::EnType::PS);
		m_cb.Create(nullptr, sizeof(SConstantBuffer));
		m_mt.seed(m_rd());
		//�P�x���o�p�̃����_�����O�^�[�Q�b�g���쐬�B
		DXGI_SAMPLE_DESC multiSampleDesc;
		ZeroMemory(&multiSampleDesc, sizeof(multiSampleDesc));
		multiSampleDesc.Count = 1;
		multiSampleDesc.Quality = 0;

		CRenderTarget& mrt = GraphicsEngine().GetMainRenderTarget();
		for (auto& rt : m_reflectionRT) {
			rt.Create(
				mrt.GetWidth() >> 1,
				mrt.GetHeight() >> 1 ,
				1,
				1,
				mrt.GetRenderTargetTextureFormat(),
				DXGI_FORMAT_UNKNOWN,
				multiSampleDesc
			);
		}
		m_blur.Init(m_reflectionRT[0].GetRenderTargetSRV(), 5.0f);
	}
	/*!
	*@brief	�`��B
	*@param[in]		rc		�����_�����O�R���e�L�Xt�B
	*/
	void CScreenSpaceReflection::Render(CRenderContext& rc, CPostEffect* postEffect)
	{

		if (m_isEnable == false) {
			return;
		}
		BeginGPUEvent(L"enRenderStep_ScreenSpaceReflection");
		rc.OMSetDepthStencilState(DepthStencilState::disable, 0);
		
		CRenderTarget& rt = postEffect->GetFinalRenderTarget();

		CRenderTarget* renderTargets[] = {
			&m_reflectionRT[m_currentRTNo]
		};
		m_currentRTNo = (m_currentRTNo + 1) % NUM_CALC_AVG_RT;
		
		
		//�萔�o�b�t�@���X�V���āA���W�X�^�ɐݒ�B
		SConstantBuffer cb;
		cb.cameraPos = MainCamera().GetPosition();
		cb.mViewProj = MainCamera().GetViewProjectionMatrix();
		cb.mViewProjInv.Inverse(MainCamera().GetViewProjectionMatrix());
		cb.mViewProjInvLastFrame = m_viewProjInvLastFrame;
		cb.rayMarchStepRate = (float)(m_mt()%1000000)/1000000.0f;
		rc.UpdateSubresource(m_cb, &cb);
		rc.PSSetConstantBuffer(0, m_cb);
		m_viewProjInvLastFrame = cb.mViewProjInv;
		CGBufferRender& gBuffer = GraphicsEngine().GetGBufferRender();

		rc.PSSetSampler(0, *CPresetSamplerState::sampler_clamp_clamp_clamp_linear);
		rc.OMSetRenderTargets(1, renderTargets);
		rc.OMSetBlendState(AlphaBlendState::trans, 0, 0xFFFFFFFF);
		rc.RSSetViewport(
			0.0f, 
			0.0f, 
			static_cast<float>(m_reflectionRT[m_currentRTNo].GetWidth()), 
			static_cast<float>(m_reflectionRT[m_currentRTNo].GetHeight())
		);
		rc.PSSetShaderResource(0, rt.GetRenderTargetSRV());
		rc.PSSetShaderResource(1, gBuffer.GetRenderTarget(enGBufferNormal).GetRenderTargetSRV());
		rc.PSSetShaderResource(2, gBuffer.GetRenderTarget(enGBufferDepth).GetRenderTargetSRV());
		rc.PSSetShaderResource(3, gBuffer.GetDepthTextureLastFrameSRV());
		rc.PSSetShader(m_psShader);
		rc.VSSetShader(m_vsShader);
		//���̓��C�A�E�g��ݒ�B
		rc.IASetInputLayout(m_vsShader.GetInputLayout());

		postEffect->DrawFullScreenQuad(rc);
		
		rc.OMSetBlendState(AlphaBlendState::disable, 0, 0xFFFFFFFF);
		m_blur.Execute(rc);
		//�߂��B
		////�����_�����O�^�[�Q�b�g��؂�ւ���B
		postEffect->ToggleFinalRenderTarget();
		{
			renderTargets[0] = &postEffect->GetFinalRenderTarget();
			rc.OMSetRenderTargets(1, renderTargets);
			rc.RSSetViewport(
				0.0f, 
				0.0f, 
				static_cast<float>(renderTargets[0]->GetWidth()), 
				static_cast<float>(renderTargets[0]->GetHeight())
			);
			rc.PSSetShaderResource(0, rt.GetRenderTargetSRV());
			rc.PSSetShaderResource(1, m_blur.GetResultSRV());
			/*for (int i = 0; i < NUM_CALC_AVG_RT; i++) {
				rc.PSSetShaderResource(1 + i, m_reflectionRT[i].GetRenderTargetSRV());

			}*/
			rc.PSSetShaderResource(2, gBuffer.GetRenderTarget(enGBufferSpecular).GetRenderTargetSRV());

			//�ŏI����
			rc.PSSetShader(m_psFinalShader);
			postEffect->DrawFullScreenQuad(rc);
			for (int i = 0; i < NUM_CALC_AVG_RT; i++) {
				rc.PSUnsetShaderResource(1 + i);
			}
		}
		rc.OMSetDepthStencilState(DepthStencilState::SceneRender, 0);
		rc.PSUnsetShaderResource(0);

		EndGPUEvent();
	}
}
