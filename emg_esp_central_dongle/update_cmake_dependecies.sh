#!/bin/bash

output="main/CMakeLists.txt"

srcs=$(find . -type f -name "*.c" ! -path "./main/build/*" ! -path "./build/*" | cut -d '/' -f 3-)

echo "idf_component_register(SRCS" > $output

for src in $srcs
do
    echo \"$src\" >> $output
done

echo "INCLUDE_DIRS \"include\")" >> $output

echo -e "Generated CMakeLists.txt\n"

cat $output
