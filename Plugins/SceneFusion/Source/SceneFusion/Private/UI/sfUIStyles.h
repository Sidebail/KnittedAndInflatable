/*************************************************************************
 *
 * KINEMATICOUP CONFIDENTIAL
 * __________________
 *
 *  Copyright (2017-2020) KinematicSoup Technologies Incorporated
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of KinematicSoup Technologies Incorporated and its
 * suppliers, if any.  The intellectual and technical concepts contained
 * herein are proprietary to KinematicSoup Technologies Incorporated
 * and its suppliers and may be covered by Canadian and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from KinematicSoup Technologies Incorporated.
 */
#pragma once

#include <CoreMinimal.h>
#include <Styling/SlateStyle.h>

class sfUIStyles
{
public:
    /**
     * Initialize.
     */
    static void Initialize();

    /**
     * Shutdown.
     */
    static void Shutdown();

    /**
     * Style name.
     *
     * @return  FName
     */
    static FName GetStyleSetName();

    /**
     * Reload slate textures.
     */
    static void ReloadTextures();

    /**
     * Return the slate style set.
     *
     * @return  const ISlateStyle&
     */
    static const ISlateStyle& Get();

    /**
     * Create an icon brush.
     *
     * @param   const FString& iconName
     * @param   const FVector2D& iconSize
     * @return  FSlateImageBrush*
     */
    static FSlateImageBrush* CreateIcon(const FString& iconName, const FVector2D& iconSize);

    /**
     * Get default font style.
     *
     * @param   const FName typefaceFontName
     * @param   const int size
     * @return  FSlateFontInfo
     */
    static FSlateFontInfo GetDefaultFontStyle(const FName typefaceFontName, const int size);

private:
    static TSharedPtr<class FSlateStyleSet> StyleInstancePtr;

    /**
     * Create the slate styles.
     *
     * @return TSharedRef<FSlateStyleSet>
     */
    static TSharedRef<class FSlateStyleSet> Create();
};