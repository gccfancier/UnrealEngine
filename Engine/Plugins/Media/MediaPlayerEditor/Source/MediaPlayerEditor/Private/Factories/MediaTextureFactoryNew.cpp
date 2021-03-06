// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaPlayerEditorPCH.h"
#include "MediaTextureFactoryNew.h"


/* UMediaTextureFactoryNew structors
 *****************************************************************************/

UMediaTextureFactoryNew::UMediaTextureFactoryNew(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UMediaTexture::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}


/* UFactory overrides
 *****************************************************************************/

UObject* UMediaTextureFactoryNew::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	auto MediaTexture = NewObject<UMediaTexture>(InParent, InClass, InName, Flags);

	if (MediaTexture != nullptr)
	{
		MediaTexture->UpdateResource();
	}

	return MediaTexture;
}


uint32 UMediaTextureFactoryNew::GetMenuCategories() const
{
	return EAssetTypeCategories::Media | EAssetTypeCategories::MaterialsAndTextures;
}


bool UMediaTextureFactoryNew::ShouldShowInNewMenu() const
{
	return true;
}
