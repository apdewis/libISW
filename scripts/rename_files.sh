#!/bin/bash
# Xaw3d to ISW File Renaming Script
# Part 1: Rename source and header files

set -e

echo "=== Xaw3d to ISW File Renaming ==="
echo

# Create ISW header directory
echo "Creating include/ISW directory..."
mkdir -p include/ISW

# Move and rename headers from X11/Xaw3d to ISW
echo "Moving headers from include/X11/Xaw3d/ to include/ISW/..."
if [ -d "include/X11/Xaw3d" ]; then
    for file in include/X11/Xaw3d/*.h; do
        if [ -f "$file" ]; then
            basename=$(basename "$file")
            # Rename Xaw prefix files
            newname=$(echo "$basename" | sed 's/^Xaw3d/ISW/' | sed 's/^Xaw/ISW/')
            cp "$file" "include/ISW/$newname"
            echo "  $file -> include/ISW/$newname"
        fi
    done
fi

# Rename source files in src/
echo
echo "Renaming source files in src/..."
for file in src/Xaw*.c src/Xaw*.h; do
    if [ -f "$file" ]; then
        basename=$(basename "$file")
        newname=$(echo "$basename" | sed 's/^Xaw3d/ISW/' | sed 's/^Xaw/ISW/')
        cp "$file" "src/$newname"
        echo "  $file -> src/$newname"
    fi
done

echo
echo "File renaming complete!"
echo "Note: Original files preserved. Review changes before deleting originals."
