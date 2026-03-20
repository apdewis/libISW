#!/bin/bash
# Xaw3d to ISW Symbol Renaming Script  
# Part 2: Rename functions, defines, and symbols

set -e

echo "=== Xaw3d to ISW Symbol Renaming ==="
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

# Phase 2: Public API Functions (XawXcb* prefix)
echo
echo "Phase 2: Renaming XCB-specific functions..."
rename_in_files "\\bXawXcbDrawImageString\\b" "ISWXcbDrawImageString" "XCB draw image string"
rename_in_files "\\bXawXcbDrawString\\b" "ISWXcbDrawString" "XCB draw string"
rename_in_files "\\bXawXcbTextWidth\\b" "ISWXcbTextWidth" "XCB text width"
rename_in_files "\\bXawXcbQueryFontMetrics\\b" "ISWXcbQueryFontMetrics" "XCB font metrics"
rename_in_files "\\bXawXcbDrawText\\b" "ISWXcbDrawText" "XCB draw text"

# Phase 3: Public API Functions (Xaw* prefix)
echo
echo "Phase 3: Renaming general Xaw functions..."
rename_in_files "\\bXawQueryPointer\\b" "ISWQueryPointer" "Query pointer"
rename_in_files "\\bXawCreateRegion\\b" "ISWCreateRegion" "Create region"
rename_in_files "\\bXawDestroyRegion\\b" "ISWDestroyRegion" "Destroy region"
rename_in_files "\\bXawUnionRectWithRegion\\b" "ISWUnionRectWithRegion" "Union rect with region"
rename_in_files "\\bXawSubtractRegion\\b" "ISWSubtractRegion" "Subtract region"
rename_in_files "\\bXawFreePixmap\\b" "ISWFreePixmap" "Free pixmap"
rename_in_files "\\bXawQueryColor\\b" "ISWQueryColor" "Query color"
rename_in_files "\\bXawAllocColor\\b" "ISWAllocColor" "Alloc color"
rename_in_files "\\bXawReleaseStippledPixmap\\b" "ISWReleaseStippledPixmap" "Release stippled pixmap"
rename_in_files "\\bXawFontStructTextWidth\\b" "ISWFontStructTextWidth" "Font struct text width"
rename_in_files "\\bXawFontSetTextWidth\\b" "ISWFontSetTextWidth" "Fontset text width"
rename_in_files "\\bXawFontTextWidth\\b" "ISWFontTextWidth" "Font text width"
rename_in_files "\\bXawFontCharWidth\\b" "ISWFontCharWidth" "Font char width"
rename_in_files "\\bXawInitGCValues\\b" "ISWInitGCValues" "Init GC values"
rename_in_files "\\bXawSetGCFont\\b" "ISWSetGCFont" "Set GC font"
rename_in_files "\\bXawSetGCForeground\\b" "ISWSetGCForeground" "Set GC foreground"
rename_in_files "\\bXawSetGCBackground\\b" "ISWSetGCBackground" "Set GC background"
rename_in_files "\\bXawSetGCGraphicsExposures\\b" "ISWSetGCGraphicsExposures" "Set GC exposures"
rename_in_files "\\bXawSetGCFunction\\b" "ISWSetGCFunction" "Set GC function"
rename_in_files "\\bXawReshapeWidget\\b" "ISWReshapeWidget" "Reshape widget"
rename_in_files "\\bXawCopyISOLatin1Lowered\\b" "ISWCopyISOLatin1Lowered" "Copy ISO Latin1"
rename_in_files "\\bXawCvtStringToOrientation\\b" "ISWCvtStringToOrientation" "Convert to orientation"
rename_in_files "\\bXawCvtStringToJustify\\b" "ISWCvtStringToJustify" "Convert to justify"
rename_in_files "\\bXawCvtStringToEdgeType\\b" "ISWCvtStringToEdgeType" "Convert to edge type"
rename_in_files "\\bXawCvtStringToWidget\\b" "ISWCvtStringToWidget" "Convert to widget"
rename_in_files "\\bXawDistinguishablePixels\\b" "ISWDistinguishablePixels" "Distinguishable pixels"
rename_in_files "\\bXawCompareISOLatin1\\b" "ISWCompareISOLatin1" "Compare ISO Latin1"
rename_in_files "\\bXawLoadFallbackFont\\b" "ISWLoadFallbackFont" "Load fallback font"
rename_in_files "\\bXawFreeFallbackFont\\b" "ISWFreeFallbackFont" "Free fallback font"
rename_in_files "\\bXawScrollbarSetThumb\\b" "ISWScrollbarSetThumb" "Scrollbar set thumb"

# Phase 4: Internal Functions (_Xaw* prefix)
echo
echo "Phase 4: Renaming internal functions..."
rename_in_files "\\b_Xaw_iswspace\\b" "_ISW_iswspace" "Internal iswspace"
rename_in_files "\\b_XawTextMBToWC\\b" "_ISWTextMBToWC" "Internal MB to WC"
rename_in_files "\\b_XawTextWCToMB\\b" "_ISWTextWCToMB" "Internal WC to MB"
rename_in_files "\\b_XawMulti" "_ISWMulti" "Internal Multi functions"
rename_in_files "\\b_Xaw3d" "_ISW" "Internal 3d functions"

# Phase 5: Type names
echo
echo "Phase 5: Renaming type names..."
rename_in_files "\\bXawFontMetrics\\b" "ISWFontMetrics" "Font metrics type"
rename_in_files "\\bXawFontSet\\b" "ISWFontSet" "FontSet type"
rename_in_files "\\bXawRegionPtr\\b" "ISWRegionPtr" "Region pointer type"
rename_in_files "\\bXawTextBlock\\b" "ISWTextBlock" "Text block type"
rename_in_files "\\bXawTextPosition\\b" "ISWTextPosition" "Text position type"

echo
echo "Symbol renaming complete!"
echo "Review changes with: git diff"
