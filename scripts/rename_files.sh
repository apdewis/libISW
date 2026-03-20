#!/bin/bash
# Isw3d to ISW File Renaming Script
# Part 1: Rename source and header files

set -e

echo "=== Isw3d to ISW File Renaming ==="
echo

# Create ISW header directory
echo "Creating include/ISW directory..."
mkdir -p include/ISW

# Move and rename headers from X11/Isw3d to ISW
echo "Moving headers from include/X11/Isw3d/ to include/ISW/..."
if [ -d "include/X11/Isw3d" ]; then
    for file in include/X11/Isw3d/*.h; do
        if [ -f "$file" ]; then
            basename=$(basename "$file")
            # Rename Isw prefix files
            newname=$(echo "$basename" | sed 's/^Isw3d/ISW/' | sed 's/^Isw/ISW/')
            cp "$file" "include/ISW/$newname"
            echo "  $file -> include/ISW/$newname"
        fi
    done
fi

# Rename source files in src/
echo
echo "Renaming source files in src/..."
for file in src/Isw*.c src/Isw*.h; do
    if [ -f "$file" ]; then
        basename=$(basename "$file")
        newname=$(echo "$basename" | sed 's/^Isw3d/ISW/' | sed 's/^Isw/ISW/')
        cp "$file" "src/$newname"
        echo "  $file -> src/$newname"
    fi
done

echo
echo "File renaming complete!"
echo "Note: Original files preserved. Review changes before deleting originals."
