#!/usr/bin/env sh

if [ "$#" -ne 2 ]; then
    printf 'Usage: %s input_file output_file\n' "$0" >&2
    exit 1
fi

infile=$1
outfile=$2

if [ ! -f "$infile" ]; then
    printf 'Error: File "%s" does not exist\n' "$infile" >&2
    exit 1
fi

filename=${infile##*/}
varname=${filename%.*}

# Match the batch script when the input file has no extension.
if [ "$varname" = "$filename" ]; then
    varname=$filename
fi

printf 'Input file: "%s"\n' "$infile"
printf 'Output file: "%s"\n' "$outfile"

if ! {
    printf 'static const char *%s = R"(\n' "$varname"
    cat "$infile"
    printf '\n)";\n\n'
    printf 'namespace ImGui\n{\n'
    printf '    const char *getShaderCode()\n    {\n'
    printf '        return %s;\n' "$varname"
    printf '    }\n}\n'
} > "$outfile"; then
    printf 'Error: Could not write output file "%s"\n' "$outfile" >&2
    exit 1
fi

printf 'Conversion completed: "%s"\n' "$outfile"
