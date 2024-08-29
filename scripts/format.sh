#!/bin/bash


# Function to format files
format_files() {
    local dir=$1
    echo "Searching for .hpp and .cpp files in directory: $dir"

    # Use 'find' to locate all .cpp and .hpp files recursively in the specified directory
    find "$dir" -type f \( -name "*.cpp" -o -name "*.hpp" \) | while read -r file; do
        echo "Formatting file: $file"
        clang-format -i "$file" # Format the file in place with clang-format
    done
}

# Format files
format_files "core"
format_files "server"
format_files "agent"

echo "Formatting complete."