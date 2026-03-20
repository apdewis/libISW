#!/bin/bash
# Isw3d to ISW Include Statement Updates
# Part 3: Update all #include statements

set -e

echo "=== Isw3d to ISW Include Statement Updates ==="
echo

# Function to update includes
update_includes() {
    local description=$1
    local pattern=$2
    local replacement=$3
    
    echo "Updating: $description"
    find . -type f \( -name "*.c" -o -name "*.h" \) \
        -not -path "./.*" -not -path "*/autom4te.cache/*" \
        -exec sed -i "s|${pattern}|${replacement}|g" {} \; 2>/dev/null || true
}

# Phase 1: Update include guards in headers
echo "Phase 1: Updating header include guards..."
find include/ISW -name "*.h" 2>/dev/null | while read file; do
    if [ -f "$file" ]; then
        basename=$(basename "$file" .h)
        # Update include guards: _Something_h -> _ISW_Something_h
        sed -i -E "s/^#ifndef _([A-Za-z0-9_]+)_h$/#ifndef _ISW_\1_h/" "$file"
        sed -i -E "s/^#define _([A-Za-z0-9_]+)_h$/#define _ISW_\1_h/" "$file"
        sed -i -E "s/^#endif.*_([A-Za-z0-9_]+)_h/#endif \/* _ISW_\1_h *\//" "$file"
    fi
done

# Phase 2: Update #include statements
echo
echo "Phase 2: Updating #include statements..."
update_includes "X11/Isw3d includes" "<X11/Isw3d/\\([^>]*\\)>" "<ISW/\\1>"
update_includes "Quoted X11/Isw3d includes" "\"X11/Isw3d/\\([^\"]*\\)\"" "\"ISW/\\1\""

# Phase 3: Update internal header includes
echo
echo "Phase 3: Updating internal header includes..."
update_includes "IswXcbDraw.h" "\"IswXcbDraw\\.h\"" "\"ISWXcbDraw.h\""
update_includes "IswI18n.h" "\"IswI18n\\.h\"" "\"ISWI18n.h\""
update_includes "Isw3dP.h" "\"Isw3dP\\.h\"" "\"ISWP.h\""
update_includes "<Isw3dP.h>" "<Isw3dP\\.h>" "<ISWP.h>"

# Phase 4: Update specific Isw header references
echo
echo "Phase 4: Updating Isw-prefixed header references..."
for header in Init Context Atoms Drawing Im; do
    update_includes "Isw${header}.h" "\"Isw${header}\\.h\"" "\"ISW${header}.h\""
    update_includes "<Isw${header}.h>" "<Isw${header}\\.h>" "<ISW${header}.h>"
done

echo
echo "Include statement updates complete!"
echo "Review changes with: git diff | grep include"
