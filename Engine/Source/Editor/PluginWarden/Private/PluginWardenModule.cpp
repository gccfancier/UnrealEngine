// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PluginWardenPrivatePCH.h"

#include "PluginWardenModule.h"
#include "SAuthorizingPlugin.h"

#include "SThrobber.h"
#include "IPortalApplicationWindow.h"
#include "IPortalServiceLocator.h"
#include "Application/IPortalApplicationWindow.h"
#include "DesktopPlatformModule.h"

IMPLEMENT_MODULE( FPluginWardenModule, PluginWarden );

#define LOCTEXT_NAMESPACE "PluginWarden"

static TSet<FString> AuthorizedPlugins;

void FPluginWardenModule::StartupModule()
{
	
}

void FPluginWardenModule::ShutdownModule()
{

}

void FPluginWardenModule::CheckEntitlementForPlugin(const FText& PluginFriendlyName, const FString& PluginGuid, TFunction<void()> AuthorizedCallback)
{
	// If we've previously authorized the plug-in, just immediately verify access.
	if ( AuthorizedPlugins.Contains(PluginGuid) )
	{
		AuthorizedCallback();
		return;
	}

	// Create the window
	TSharedRef<SWindow> AuthorizingPluginWindow = SNew(SWindow)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.HasCloseButton(true)
		.SizingRule(ESizingRule::Autosized)
		.Title(FText::Format(LOCTEXT("EntitlementCheckFormat", "{0} - Entitlement Check"), PluginFriendlyName));

	TSharedRef<SAuthorizingPlugin> PluginAuthPanel = SNew(SAuthorizingPlugin, AuthorizingPluginWindow, PluginFriendlyName, PluginGuid, AuthorizedCallback);

	AuthorizingPluginWindow->SetContent(PluginAuthPanel);

	FSlateApplication::Get().AddModalWindow(AuthorizingPluginWindow, nullptr);
}

#undef LOCTEXT_NAMESPACE
