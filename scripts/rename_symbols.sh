#!/bin/bash
# Isw3d to ISW Symbol Renaming Script  
# Part 2: Rename functions, defines, and symbols

set -e

echo "=== Isw3d to ISW Symbol Renaming ==="
echo

# Function to perform sed replacements safely
rename_in_files() {
    local pattern=$1
    local replacement=$2
    local description=$3
    
    echo "Renaming: $description"
    find . -type f \( -name "*.c" -o -name "*.h" -o -name "*.ac" -o -name "*.am" -o -name "*.md" \) \
        -not -path "./.*" -not -path "*/autom4te.cache/*" \
        -exec sed -i "s/${pattern}/${replacement}/g" {} \; 2>/dev/null || true
}

# Phase 1: Preprocessor Defines
echo "Phase 1: Renaming preprocessor defines..."
rename_in_files "XAW3D_CPPFLAGS" "ISW_CPPFLAGS" "Build variables"
rename_in_files "XAW_ARROW_SCROLLBARS" "ISW_ARROW_SCROLLBARS" "Arrow scrollbars flag"
rename_in_files "XAW_INTERNATIONALIZATION" "ISW_INTERNATIONALIZATION" "Internationalization flag"
rename_in_files "XAW_HAS_XIM" "ISW_HAS_XIM" "XIM support flag"
rename_in_files "XAW_MULTIPLANE_PIXMAPS" "ISW_MULTIPLANE_PIXMAPS" "XPM support flag"
rename_in_files "XAW_GRAY_BLKWHT_STIPPLES" "ISW_GRAY_BLKWHT_STIPPLES" "Gray stipples flag"
rename_in_files "XAW_USE_XFT" "ISW_USE_XFT" "Xft rendering flag"

# Phase 2: Public API Functions (IswXcb* prefix)
echo
echo "Phase 2: Renaming XCB-specific functions..."
rename_in_files "\\bIswXcbDrawImageString\\b" "ISWXcbDrawImageString" "XCB draw image string"
rename_in_files "\\bIswXcbDrawString\\b" "ISWXcbDrawString" "XCB draw string"
rename_in_files "\\bIswXcbTextWidth\\b" "ISWXcbTextWidth" "XCB text width"
rename_in_files "\\bIswXcbQueryFontMetrics\\b" "ISWXcbQueryFontMetrics" "XCB font metrics"
rename_in_files "\\bIswXcbDrawText\\b" "ISWXcbDrawText" "XCB draw text"

# Phase 3: Public API Functions (Isw* prefix)
echo
echo "Phase 3: Renaming general Isw functions..."
rename_in_files "\\bIswQueryPointer\\b" "ISWQueryPointer" "Query pointer"
rename_in_files "\\bIswCreateRegion\\b" "ISWCreateRegion" "Create region"
rename_in_files "\\bIswDestroyRegion\\b" "ISWDestroyRegion" "Destroy region"
rename_in_files "\\bIswUnionRectWithRegion\\b" "ISWUnionRectWithRegion" "Union rect with region"
rename_in_files "\\bIswSubtractRegion\\b" "ISWSubtractRegion" "Subtract region"
rename_in_files "\\bIswFreePixmap\\b" "ISWFreePixmap" "Free pixmap"
rename_in_files "\\bIswQueryColor\\b" "ISWQueryColor" "Query color"
rename_in_files "\\bIswAllocColor\\b" "ISWAllocColor" "Alloc color"
rename_in_files "\\bIswReleaseStippledPixmap\\b" "ISWReleaseStippledPixmap" "Release stippled pixmap"
rename_in_files "\\bIswFontStructTextWidth\\b" "ISWFontStructTextWidth" "Font struct text width"
rename_in_files "\\bIswFontSetTextWidth\\b" "ISWFontSetTextWidth" "Fontset text width"
rename_in_files "\\bIswFontTextWidth\\b" "ISWFontTextWidth" "Font text width"
rename_in_files "\\bIswFontCharWidth\\b" "ISWFontCharWidth" "Font char width"
rename_in_files "\\bIswInitGCValues\\b" "ISWInitGCValues" "Init GC values"
rename_in_files "\\bIswSetGCFont\\b" "ISWSetGCFont" "Set GC font"
rename_in_files "\\bIswSetGCForeground\\b" "ISWSetGCForeground" "Set GC foreground"
rename_in_files "\\bIswSetGCBackground\\b" "ISWSetGCBackground" "Set GC background"
rename_in_files "\\bIswSetGCGraphicsExposures\\b" "ISWSetGCGraphicsExposures" "Set GC exposures"
rename_in_files "\\bIswSetGCFunction\\b" "ISWSetGCFunction" "Set GC function"
rename_in_files "\\bIswReshapeWidget\\b" "ISWReshapeWidget" "Reshape widget"
rename_in_files "\\bIswCopyISOLatin1Lowered\\b" "ISWCopyISOLatin1Lowered" "Copy ISO Latin1"
rename_in_files "\\bIswCvtStringToOrientation\\b" "ISWCvtStringToOrientation" "Convert to orientation"
rename_in_files "\\bIswCvtStringToJustify\\b" "ISWCvtStringToJustify" "Convert to justify"
rename_in_files "\\bIswCvtStringToEdgeType\\b" "ISWCvtStringToEdgeType" "Convert to edge type"
rename_in_files "\\bIswCvtStringToWidget\\b" "ISWCvtStringToWidget" "Convert to widget"
rename_in_files "\\bIswDistinguishablePixels\\b" "ISWDistinguishablePixels" "Distinguishable pixels"
rename_in_files "\\bIswCompareISOLatin1\\b" "ISWCompareISOLatin1" "Compare ISO Latin1"
rename_in_files "\\bIswLoadFallbackFont\\b" "ISWLoadFallbackFont" "Load fallback font"
rename_in_files "\\bIswFreeFallbackFont\\b" "ISWFreeFallbackFont" "Free fallback font"
rename_in_files "\\bIswScrollbarSetThumb\\b" "ISWScrollbarSetThumb" "Scrollbar set thumb"

# Phase 4: Internal Functions (_Isw* prefix)
echo
echo "Phase 4: Renaming internal functions..."
rename_in_files "\\b_Isw_iswspace\\b" "_ISW_iswspace" "Internal iswspace"
rename_in_files "\\b_IswTextMBToWC\\b" "_ISWTextMBToWC" "Internal MB to WC"
rename_in_files "\\b_IswTextWCToMB\\b" "_ISWTextWCToMB" "Internal WC to MB"
rename_in_files "\\b_IswMulti" "_ISWMulti" "Internal Multi functions"
rename_in_files "\\b_Isw3d" "_ISW" "Internal 3d functions"

# Phase 5: Type names
echo
echo "Phase 5: Renaming type names..."
rename_in_files "\\bIswFontMetrics\\b" "ISWFontMetrics" "Font metrics type"
rename_in_files "\\bIswFontSet\\b" "ISWFontSet" "FontSet type"
rename_in_files "\\bIswRegionPtr\\b" "ISWRegionPtr" "Region pointer type"
rename_in_files "\\bIswTextBlock\\b" "ISWTextBlock" "Text block type"
rename_in_files "\\bIswTextPosition\\b" "ISWTextPosition" "Text position type"

echo
echo "Symbol renaming complete!"
echo "Review changes with: git diff"
