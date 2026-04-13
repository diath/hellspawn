#!/usr/bin/env bash
shopt -s globstar

JOB_COUNT=$(nproc --ignore=2)

if [ -d "build_clang" ]; then
	BUILD_DIR="build_clang/"
else
	BUILD_DIR="build/"
fi

run-clang-tidy -j$JOB_COUNT -p "$BUILD_DIR" src/**/*.cpp
