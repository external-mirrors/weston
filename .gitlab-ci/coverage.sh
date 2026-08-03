#!/bin/bash
set -xe

source "${FDO_CI_BASH_HELPERS}"

cd "$BUILDDIR"

if [ x"$BUILD_COVERAGE" = "xy" ] ; then
        fdo_log_section_start_collapsed coverage "coverage"
        ninja -k0 -j${FDO_CI_CONCURRENT:-4} coverage-html > meson-logs/ninja-coverage-html.txt
        ninja -k0 -j${FDO_CI_CONCURRENT:-4} coverage-xml
        fdo_log_section_end coverage
fi
