// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DecalRenderingShared.cpp
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "DecalRenderingShared.h"

static TAutoConsoleVariable<float> CVarDecalFadeScreenSizeMultiplier(
	TEXT("r.Decal.FadeScreenSizeMult"),
	1.0f,
	TEXT("Control the per decal fade screen size. Multiplies with the per-decal screen size fade threshold.")
	TEXT("  Smaller means decals fade less aggressively.")
	);

static bool IsBlendModeSupported(EShaderPlatform Platform, EDecalBlendMode DecalBlendMode)
{
	if (IsMobilePlatform(Platform))
	{
		switch (DecalBlendMode)
		{
			case DBM_Stain:			// Modulate
			case DBM_Emissive:		// Additive
			case DBM_Translucent:	// Translucent
				break;
			default:
				return false;
		}
	}

	return true;
}

static EDecalBlendMode ComputeFinalDecalBlendMode(EShaderPlatform Platform, EDecalBlendMode DecalBlendMode, bool bUseNormal)
{
	if (!bUseNormal)
	{
		if(DecalBlendMode == DBM_DBuffer_ColorNormalRoughness)
		{
			DecalBlendMode = DBM_DBuffer_ColorRoughness;
		}
		else if(DecalBlendMode == DBM_DBuffer_NormalRoughness)
		{
			DecalBlendMode = DBM_DBuffer_Roughness;
		}
	}
		
	return DecalBlendMode;
}

FTransientDecalRenderData::FTransientDecalRenderData(const FScene& InScene, const FDeferredDecalProxy* InDecalProxy, float InConservativeRadius)
	: DecalProxy(InDecalProxy)
	, FadeAlpha(1.0f)
	, ConservativeRadius(InConservativeRadius)
{
	MaterialProxy = InDecalProxy->DecalMaterial->GetRenderProxy(InDecalProxy->bOwnerSelected);
	MaterialResource = MaterialProxy->GetMaterial(InScene.GetFeatureLevel());
	check(MaterialProxy && MaterialResource);
	bHasNormal = MaterialResource->HasNormalConnected();
	DecalBlendMode = ComputeFinalDecalBlendMode(InScene.GetShaderPlatform(), (EDecalBlendMode)MaterialResource->GetDecalBlendMode(), bHasNormal);
}

/**
 * A vertex shader for projecting a deferred decal onto the scene.
 */
class FDeferredDecalVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FDeferredDecalVS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	FDeferredDecalVS( )	{ }
	FDeferredDecalVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		FrustumComponentToClip.Bind(Initializer.ParameterMap, TEXT("FrustumComponentToClip"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FMatrix& InFrustumComponentToClip)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		SetShaderValue(RHICmdList, ShaderRHI, FrustumComponentToClip, InFrustumComponentToClip);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << FrustumComponentToClip;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter FrustumComponentToClip;
};

IMPLEMENT_SHADER_TYPE(,FDeferredDecalVS,TEXT("DeferredDecal"),TEXT("MainVS"),SF_Vertex);

/**
 * A pixel shader for projecting a deferred decal onto the scene.
 */
class FDeferredDecalPS : public FMaterialShader
{
	DECLARE_SHADER_TYPE(FDeferredDecalPS,Material);
public:

	/**
	  * Makes sure only shaders for materials that are explicitly flagged
	  * as 'UsedAsDeferredDecal' in the Material Editor gets compiled into
	  * the shader cache.
	  */
	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material)
	{
		return Material->IsUsedWithDeferredDecal();
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment )
	{
		FMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);

		check(Material);
		
		EDecalBlendMode DecalBlendMode = ComputeFinalDecalBlendMode(Platform, (EDecalBlendMode)Material->GetDecalBlendMode(), Material->HasNormalConnected());
		FDecalRendering::ERenderTargetMode RenderTargetMode = FDecalRendering::ComputeRenderTargetMode(Platform, DecalBlendMode);
		uint32 RenderTargetCount = FDecalRendering::ComputeRenderTargetCount(Platform, RenderTargetMode);

		// avoid using the index directly, better use DECALBLENDMODEID_VOLUMETRIC, DECALBLENDMODEID_STAIN, ...
		OutEnvironment.SetDefine(TEXT("DECAL_BLEND_MODE"), (int32)DecalBlendMode);
		OutEnvironment.SetDefine(TEXT("DECAL_PROJECTION"), 1);
		OutEnvironment.SetDefine(TEXT("DECAL_RENDERTARGET_COUNT"), RenderTargetCount);
		OutEnvironment.SetDefine(TEXT("DECAL_RENDERSTAGE"), (int32)FDecalRendering::ComputeRenderStage(Platform, DecalBlendMode));

		// to compare against DECAL_BLEND_MODE, we can expose more if needed
		OutEnvironment.SetDefine(TEXT("DECALBLENDMODEID_VOLUMETRIC"), (int32)DBM_Volumetric_DistanceFunction);
		OutEnvironment.SetDefine(TEXT("DECALBLENDMODEID_STAIN"), (int32)DBM_Stain);
		OutEnvironment.SetDefine(TEXT("DECALBLENDMODEID_NORMAL"), (int32)DBM_Normal);
	}

	FDeferredDecalPS() {}
	FDeferredDecalPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FMaterialShader(Initializer)
	{
		ScreenToDecal.Bind(Initializer.ParameterMap,TEXT("ScreenToDecal"));
		DecalToWorld.Bind(Initializer.ParameterMap,TEXT("DecalToWorld"));
		FadeAlpha.Bind(Initializer.ParameterMap, TEXT("FadeAlpha"));
		WorldToDecal.Bind(Initializer.ParameterMap,TEXT("WorldToDecal"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FMaterialRenderProxy* MaterialProxy, const FDeferredDecalProxy& DecalProxy, const float FadeAlphaValue=1.0f)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FMaterialShader::SetParameters(RHICmdList, ShaderRHI, MaterialProxy, *MaterialProxy->GetMaterial(View.GetFeatureLevel()), View, true, ESceneRenderTargetsMode::SetTextures);

		FTransform ComponentTrans = DecalProxy.ComponentTrans;

		FMatrix WorldToComponent = ComponentTrans.ToInverseMatrixWithScale();

		// Set the transform from screen space to light space.
		if(ScreenToDecal.IsBound())
		{
			const FMatrix ScreenToDecalValue = 
				FMatrix(
					FPlane(1,0,0,0),
					FPlane(0,1,0,0),
					FPlane(0,0,View.ViewMatrices.ProjMatrix.M[2][2],1),
					FPlane(0,0,View.ViewMatrices.ProjMatrix.M[3][2],0)
				) * View.InvViewProjectionMatrix * WorldToComponent;

			SetShaderValue(RHICmdList, ShaderRHI, ScreenToDecal, ScreenToDecalValue);
		}

		// Set the transform from light space to world space
		if(DecalToWorld.IsBound())
		{
			const FMatrix DecalToWorldValue = ComponentTrans.ToMatrixWithScale();
			
			SetShaderValue(RHICmdList, ShaderRHI, DecalToWorld, DecalToWorldValue);
		}

		SetShaderValue(RHICmdList, ShaderRHI, FadeAlpha, FadeAlphaValue);
		SetShaderValue(RHICmdList, ShaderRHI, WorldToDecal, WorldToComponent);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMaterialShader::Serialize(Ar);
		Ar << ScreenToDecal << DecalToWorld << WorldToDecal << FadeAlpha;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter ScreenToDecal;
	FShaderParameter DecalToWorld;
	FShaderParameter FadeAlpha;
	FShaderParameter WorldToDecal;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FDeferredDecalPS,TEXT("DeferredDecal"),TEXT("MainPS"),SF_Pixel);


void FDecalRendering::BuildVisibleDecalList(const FScene& Scene, const FViewInfo& View, EDecalRenderStage DecalRenderStage, FTransientDecalRenderDataList& OutVisibleDecals)
{
	OutVisibleDecals.Empty(Scene.Decals.Num());

	const float FadeMultiplier = CVarDecalFadeScreenSizeMultiplier.GetValueOnRenderThread();
	const EShaderPlatform ShaderPlatform = View.GetShaderPlatform();
	
	// Build a list of decals that need to be rendered for this view in SortedDecals
	for (const FDeferredDecalProxy* DecalProxy : Scene.Decals)
	{
		bool bIsShown = true;

		// Handle the decal actor having bHidden set when we are in the editor, in G mode
#if WITH_EDITOR
		if (View.Family->EngineShowFlags.Editor)
#endif
		{
			if (!DecalProxy->DrawInGame)
			{
				bIsShown = false;
			}
		}

		const FMatrix ComponentToWorldMatrix = DecalProxy->ComponentTrans.ToMatrixWithScale();

		// can be optimized as we test against a sphere around the box instead of the box itself
		const float ConservativeRadius = FMath::Sqrt(
				ComponentToWorldMatrix.GetScaledAxis(EAxis::X).SizeSquared() +
				ComponentToWorldMatrix.GetScaledAxis(EAxis::Y).SizeSquared() +
				ComponentToWorldMatrix.GetScaledAxis(EAxis::Z).SizeSquared());

		// can be optimized as the test is too conservative (sphere instead of OBB)
		if(ConservativeRadius < SMALL_NUMBER || !View.ViewFrustum.IntersectSphere(ComponentToWorldMatrix.GetOrigin(), ConservativeRadius))
		{
			bIsShown = false;
		}

		if (bIsShown)
		{
			FTransientDecalRenderData Data(Scene, DecalProxy, ConservativeRadius);
			
			// filter out decals with blend modes that are not supported on current platform
			if (IsBlendModeSupported(ShaderPlatform, Data.DecalBlendMode))
			{
				if (Data.DecalProxy->Component->FadeScreenSize != 0.0f)
				{
					float Distance = (View.ViewMatrices.ViewOrigin - ComponentToWorldMatrix.GetOrigin()).Size();
					float Radius = ComponentToWorldMatrix.GetMaximumAxisScale();
					float CurrentScreenSize = ((Radius / Distance) * FadeMultiplier);

					// fading coefficient needs to increase with increasing field of view and decrease with increasing resolution
					// FadeCoeffScale is an empirically determined constant to bring us back roughly to fraction of screen size for FadeScreenSize
					const float FadeCoeffScale = 600.0f;
					float FOVFactor = ((2.0f/View.ViewMatrices.ProjMatrix.M[0][0]) / View.ViewRect.Width()) * FadeCoeffScale;
					float FadeCoeff = Data.DecalProxy->Component->FadeScreenSize * FOVFactor;
					float FadeRange = FadeCoeff * 0.5f;

					float Alpha = (CurrentScreenSize - FadeCoeff) / FadeRange;
					Data.FadeAlpha = FMath::Min(Alpha, 1.0f);
				}

				EDecalRenderStage LocalDecalRenderStage = ComputeRenderStage(ShaderPlatform, Data.DecalBlendMode);

				// we could do this test earlier to avoid the decal intersection but getting DecalBlendMode also costs
				if (View.Family->EngineShowFlags.ShaderComplexity || (DecalRenderStage == LocalDecalRenderStage && Data.FadeAlpha>0.0f) )
				{
					OutVisibleDecals.Add(Data);
				}
			}
		}
	}

	if (OutVisibleDecals.Num() > 0)
	{
		// Sort by sort order to allow control over composited result
		// Then sort decals by state to reduce render target switches
		// Also sort by component since Sort() is not stable
		struct FCompareFTransientDecalRenderData
		{
			FORCEINLINE bool operator()(const FTransientDecalRenderData& A, const FTransientDecalRenderData& B) const
			{
				if (B.DecalProxy->SortOrder != A.DecalProxy->SortOrder)
				{ 
					return A.DecalProxy->SortOrder < B.DecalProxy->SortOrder;
				}
				if (B.DecalBlendMode != A.DecalBlendMode)
				{
					return (int32)B.DecalBlendMode < (int32)A.DecalBlendMode;
				}
				if (B.bHasNormal != A.bHasNormal)
				{
					return B.bHasNormal < A.bHasNormal;
				}
				// Batch decals with the same material together
				if (B.MaterialProxy != A.MaterialProxy)
				{
					return B.MaterialProxy < A.MaterialProxy;
				}
				return (PTRINT)B.DecalProxy->Component < (PTRINT)A.DecalProxy->Component;
			}
		};

		// Sort decals by blend mode to reduce render target switches
		OutVisibleDecals.Sort(FCompareFTransientDecalRenderData());
	}
}

FMatrix FDecalRendering::ComputeComponentToClipMatrix(const FViewInfo& View, const FMatrix& DecalComponentToWorld)
{
	FMatrix ComponentToWorldMatrixTrans = DecalComponentToWorld.ConcatTranslation(View.ViewMatrices.PreViewTranslation);
	return ComponentToWorldMatrixTrans * View.ViewMatrices.TranslatedViewProjectionMatrix;
}

FDecalRendering::ERenderTargetMode FDecalRendering::ComputeRenderTargetMode(EShaderPlatform Platform, EDecalBlendMode DecalBlendMode)
{
	if (IsMobilePlatform(Platform))
	{
		return RTM_SceneColor;
	}
	
	switch(DecalBlendMode)
	{
		case DBM_Translucent:
		case DBM_Stain:
			return RTM_SceneColorAndGBuffer;

		case DBM_Normal:
			return RTM_GBufferNormal;

		case DBM_Emissive:
			return RTM_SceneColor;

		case DBM_DBuffer_ColorNormalRoughness:
		case DBM_DBuffer_Color:
		case DBM_DBuffer_ColorNormal:
		case DBM_DBuffer_ColorRoughness:
		case DBM_DBuffer_Normal:
		case DBM_DBuffer_NormalRoughness:
		case DBM_DBuffer_Roughness:
			// can be optimized using less MRT when possible
			return RTM_DBuffer;

		case DBM_Volumetric_DistanceFunction:
			return RTM_SceneColorAndGBufferDepthWrite;
	}

	// add the missing decal blend mode to the switch
	check(0);
	return RTM_Unknown;
}

// @return see EDecalRenderStage
EDecalRenderStage FDecalRendering::ComputeRenderStage(EShaderPlatform Platform, EDecalBlendMode DecalBlendMode)
{
	if (IsMobilePlatform(Platform))
	{
		return DRS_ForwardShading;
	}
		
	switch(DecalBlendMode)
	{
		case DBM_DBuffer_ColorNormalRoughness:
		case DBM_DBuffer_Color:
		case DBM_DBuffer_ColorNormal:
		case DBM_DBuffer_ColorRoughness:
		case DBM_DBuffer_Normal:
		case DBM_DBuffer_NormalRoughness:
		case DBM_DBuffer_Roughness:
			return DRS_BeforeBasePass;

		case DBM_Translucent:
		case DBM_Stain:
		case DBM_Normal:
		case DBM_Emissive:
			return DRS_BeforeLighting;
		
		case DBM_Volumetric_DistanceFunction:
			return DRS_AfterBasePass;

		default:
			check(0);
	}
	
	return DRS_BeforeBasePass;
}

// @return DECAL_RENDERTARGET_COUNT for the shader
uint32 FDecalRendering::ComputeRenderTargetCount(EShaderPlatform Platform, ERenderTargetMode RenderTargetMode)
{
	// has to be SceneColor on mobile 
	check(!IsMobilePlatform(Platform) || RenderTargetMode == RTM_SceneColor);

	switch(RenderTargetMode)
	{
		case RTM_SceneColorAndGBuffer:				return 4;
		case RTM_SceneColorAndGBufferDepthWrite:	return 4;
		case RTM_DBuffer:							return 3;
		case RTM_GBufferNormal:						return 1;
		case RTM_SceneColor:						return 1;
	}

	return 0;
}

void FDecalRendering::SetShader(FRHICommandList& RHICmdList, const FViewInfo& View, bool bShaderComplexity, const FTransientDecalRenderData& DecalData, const FMatrix& FrustumComponentToClip)
{
	const FMaterialShaderMap* MaterialShaderMap = DecalData.MaterialResource->GetRenderingThreadShaderMap();
	auto PixelShader = MaterialShaderMap->GetShader<FDeferredDecalPS>();
	TShaderMapRef<FDeferredDecalVS> VertexShader(View.ShaderMap);

	// we don't have the Primitive uniform buffer setup for decals (later we want to batch)
	{
		auto& PrimitiveVS = VertexShader->GetUniformBufferParameter<FPrimitiveUniformShaderParameters>();
		auto& PrimitivePS = PixelShader->GetUniformBufferParameter<FPrimitiveUniformShaderParameters>();

		// uncomment to track down usage of the Primitive uniform buffer
		//	check(!PrimitiveVS.IsBound());
		//	check(!PrimitivePS.IsBound());

		// to prevent potential shader error (UE-18852 ElementalDemo crashes due to nil constant buffer)
		SetUniformBufferParameter(RHICmdList, VertexShader->GetVertexShader(), PrimitiveVS, GIdentityPrimitiveUniformBuffer);
		SetUniformBufferParameter(RHICmdList, PixelShader->GetPixelShader(), PrimitivePS, GIdentityPrimitiveUniformBuffer);
	}

	if(bShaderComplexity)
	{
		TShaderMapRef<FShaderComplexityAccumulatePS> VisualizePixelShader(View.ShaderMap);
		const uint32 NumPixelShaderInstructions = PixelShader->GetNumInstructions();
		const uint32 NumVertexShaderInstructions = VertexShader->GetNumInstructions();

		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GetVertexDeclarationFVector4(), *VertexShader, *VisualizePixelShader);

		VisualizePixelShader->SetParameters(RHICmdList, NumVertexShaderInstructions, NumPixelShaderInstructions, View.GetFeatureLevel());
	}
	else
	{
		// first Bind, then SetParameters()
		RHICmdList.SetLocalBoundShaderState(RHICmdList.BuildLocalBoundShaderState(GetVertexDeclarationFVector4(), VertexShader->GetVertexShader(), FHullShaderRHIRef(), FDomainShaderRHIRef(), PixelShader->GetPixelShader(), FGeometryShaderRHIRef()));
		
		PixelShader->SetParameters(RHICmdList, View, DecalData.MaterialProxy, *DecalData.DecalProxy, DecalData.FadeAlpha);
	}

	VertexShader->SetParameters(RHICmdList, View, FrustumComponentToClip);
}

void FDecalRendering::SetVertexShaderOnly(FRHICommandList& RHICmdList, const FViewInfo& View, const FMatrix& FrustumComponentToClip)
{
	TShaderMapRef<FDeferredDecalVS> VertexShader(View.ShaderMap);
	RHICmdList.SetLocalBoundShaderState(RHICmdList.BuildLocalBoundShaderState(GetVertexDeclarationFVector4(), VertexShader->GetVertexShader(), FHullShaderRHIRef(), FDomainShaderRHIRef(), NULL, FGeometryShaderRHIRef()));
	VertexShader->SetParameters(RHICmdList, View, FrustumComponentToClip);
}
