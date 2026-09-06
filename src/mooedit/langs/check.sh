#!/bin/sh
# Validate the language definitions and the style schemes in this directory.
#
#   ./check.sh             validate every .lang and .xml here
#   ./check.sh files...    validate the files named
#
# Both schemas are gtksourceview's, like the .lang files themselves. styles.rng
# is deliberately wider than what this fork reads: it allows scale and the
# PangoUnderline names, which GtkSourceStyle here ignores and misreads. It
# catches malformed schemes, not unsupported attributes.

if [ $# -gt 0 ]; then
    files=$@
else
    cd "$(dirname "$0")" || exit 1
    files="*.lang *.xml"
fi

status=0

for file in $files; do
    case $file in
    *.xml) schema=styles.rng    ;;
    *)     schema=language2.rng ;;
    esac

    if ! output=$(xmllint --relaxng "$schema" --noout "$file" 2>&1); then
        echo "$output" >&2
        status=1
    fi
done

[ $status -eq 0 ] && echo "$(echo $files | wc -w) files, all valid"

exit $status
