#!/bin/bash
#
# TypeART library
#
# Copyright (c) 2017-2022 TypeART Authors
# Distributed under the BSD 3-Clause license.
# (See accompanying file LICENSE.txt or copy at
# https://opensource.org/licenses/BSD-3-Clause)
#
# Project home: https://github.com/tudasc/TypeART
#
# SPDX-License-Identifier: BSD-3-Clause
#

O3_passes='annotation2metadata,cross-dso-cfi,globaldce,forceattrs,inferattrs,callsite-splitting,pgo-icall-prom,ipsccp,called-value-propagation,function-attrs,rpo-function-attrs,globalsplit,wholeprogramdevirt,globalopt,mem2reg,constmerge,deadargelim,aggressive-instcombine,instcombine,inliner-wrapper,globalopt,globaldce,argpromotion,instcombine,jump-threading,sroa,tailcallelim,function-attrs,require<globals-aa>,function(invalidate<aa>),loop-simplify,lcssa,gvn,memcpyopt,dse,mldst-motion,loop-simplify,lcssa,loop-distribute,loop-vectorize,loop-unroll,transform-warning,instcombine,simplifycfg,sccp,instcombine,bdce,slp-vectorizer,vector-combine,alignment-from-assumptions,instcombine,jump-threading,lowertypetests,lowertypetests,simplifycfg,elim-avail-extern,globaldce,annotation-remarks'
O3_with_genCC_passes=$O3_passes",genCC"

function global_init() {
  local -r gencc_use_relative_path=0
  if [ "$gencc_use_relative_path" == 0 ]; then
    local -r gencc_plugin=/media/tim/Volume/Studium/Master/Semester3/MasterArbeit/Code/gencc/cmake-build-debug/lib/libplugin.so
    local -r gencc_plugin_dir=$(dirname "${gencc_plugin}")
    local -r gencc_runtime=libgenCCRT.so
    local -r gencc_runtime_dir=$(dirname "${gencc_runtime}")
    local -r gencc_pipeline=$O3_with_genCC_passes
  else
    # shellcheck disable=SC2155
    local -r gencc_plugin_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    # shellcheck disable=SC2155
    local -r gencc_install_dir="$(dirname "${gencc_plugin_dir}")"
    local -r gencc_runtime_dir="${gencc_install_dir}/"
    local -r gencc_include_dir="-I${gencc_install_dir}//CallGraphGeneration"
    local -r gencc_pipeline=$O3_with_genCC_passes
  fi

  readonly compiler=clang-13
  readonly opt_tool=opt-13
  readonly llc_tool=llc-13
  readonly plugin_compatible_linker=/media/tim/Volume/Studium/Master/Semester3/MasterArbeit/LLVMRepo/llvm-project/lld/cmake-build-debug/bin/ld.lld

  readonly gencc_includes="${gencc_include_dir}"
  readonly gencc_ldflags="-L${gencc_runtime_dir}/ \
                   -Wl,-rpath,${gencc_runtime_dir}/ \
                   -l ${gencc_runtime}"

  # shellcheck disable=SC2027
  readonly gencc_plugin_flags="--lto-load-pass-plugin="${gencc_plugin_dir}/${gencc_plugin}" --lto-newpm-passes="${O3_with_genCC_passes}

  #check for optimization level and set?
  #readonly gencc_OPT_LEVEL
}

function is_wrapper_disabled() {
  case "${TYPEART_WRAPPER}" in
  off | OFF | 0 | false | FALSE)
    return 1
    ;;
  esac

  return 0
}

function is_linking() {
  for arg in "$@"; do
    case "$arg" in
    -c | -S | -E)
      return 0
    ;;
    esac
  done
  return 1
}

function has_source() {
  for arg in "$@"; do
    local extension_of_arg="${arg##*.}"
    case "$extension_of_arg" in
    cpp | cxx | cc | c)
      return 1
    ;;
    esac
  done
  return 0
}

function is_preprocessor_run() {
  # -E inline header; -M list (all) headers; -MM list file deps
  for arg in "$@"; do
    case "$arg" in
    -E | -M | -MM)
      return 1
    ;;
    esac
  done
  return 0
}

function try_extract_source() {
  # $1 == flag (source file); $2 == shift value
  local -r extension="${1##*.}"
  local -r shift_val="$2"

  case "$extension" in
  cpp | cxx | cc | c)
    source_file="$1"
    found_src_file=1
    return "$shift_val"
    ;;
  *)
    return 1
    ;;
  esac
}

function handle_source_flag() {
  if [ -n "$2" ]; then
    try_extract_source "$2" 2
  else
    try_extract_source "$1" 1
  fi
  return $?
}

function try_extract_object() {
  # $1 == flag (obj file); $2 == shift value
  local -r extension="${1##*.}"
  local -r shift_val="$2"

  case "$extension" in
  o)
    object_file="$1"
    found_obj_file=1
    return "$shift_val"
    ;;
  *)
    return 1
    ;;
  esac
}

function handle_object_flag() {
  if [ -n "$2" ]; then
    try_extract_object "$2" 2
  else
    try_extract_object "$1" 1
  fi
  return $?
}

function handle_binary() {
  if [ -n "$2" ]; then
    exe_file="$2"
    found_exe_file=1
  fi
  return 2
}

# shellcheck disable=SC2034
function parse_cmd_line() {
  found_src_file=0
  found_obj_file=0
  found_exe_file=0
  found_fpic=0
  typeart_to_asm=0
  exe_file=""
  source_file=""
  object_file=""
  asm_file=""
  ta_more_args=""
  optimize=""

  while (("$#")); do
    case "$1" in
    -O?)
      optimize=$1
      echo $optimize
      shift
      ;;
    -MT)
      if [ -n "$2" ]; then
        ta_more_args+=" $1 $2"
        shift 2
      else
        ta_more_args+=" $1"
        shift 1
      fi
      ;;
    -c | -S)
      # -S compile to asm
      if [ "$1" == "-S" ]; then
        typeart_to_asm=1
      fi
      handle_source_flag "$1" "$2"
      shift $?
      ;;
    *.s)
      asm_file="$1"
      shift 1
      ;;
    *.cpp | *.cxx | *.cc | *.c)
      handle_source_flag "$1"
      shift $?
      ;;
    -o)
      # shellcheck disable=SC2154
      if [ "$linking" == 1 ]; then
        handle_binary "$1" "$2"
      else
        handle_object_flag "$1" "$2"
      fi
      shift $?
      ;;
    *.o)
      if [ "$linking" == 0 ]; then
        handle_object_flag "$1"
        shift $?
      else
        # when linking, we don't care about object files
        ta_more_args="$ta_more_args $1"
        shift
      fi
      ;;
    -fPIC)
      # llc requires special flag
      found_fpic=1
      ta_more_args="$ta_more_args $1"
      shift
      ;;
    *) # preserve other arguments
      ta_more_args="$ta_more_args $1"
      shift
      ;;
    esac
  done
  # set other positional arguments in their proper place
  eval set -- "ta_more_args"

  if [ -z "${optimize}" ]; then
    optimize=-O0
  fi
}

function main_link() {
  # shellcheck disable=SC2086 disable=SC2068
  $compiler ${gencc_includes} ${gencc_ldflags} ${typeart_san_flags} $@
}

function main_compile() {
  if [ -z "${object_file}" ]; then
    # if no object file is specified, use filename(source_file).o
    object_file="${source_file%.*}".o
  fi

  if [ "$typeart_to_asm" == 1 ] && [ -z "${asm_file}" ]; then
    # if no asm file is specified, use filename(source_file).s
    local asm_file="${source_file%.*}".s
  fi

  local out_file="${object_file}"
  local llc_flags="--filetype=obj"

  if [ "$typeart_to_asm" == 1 ]; then
    local llc_flags="--filetype=asm"
    local out_file="${asm_file}"
  fi

  if [ "$found_fpic" == 1 ]; then
    local llc_flags="$llc_flags --relocation-model=pic"
  fi

  # shellcheck disable=SC2086
  $compiler ${ta_more_args} ${gencc_includes} ${typeart_san_flags} \
    -O1 -Xclang -disable-llvm-passes -S -emit-llvm "${source_file}" -o - |
    $opt_tool ${gencc_plugin} ${typeart_heap_mode_args} |
    $opt_tool ${optimize} -S |
    $opt_tool ${gencc_plugin} ${typeart_stack_mode_args} |
    $llc_tool -x=ir ${llc_flags} -o "${out_file}"
}

function main_in() {
  global_init

  is_wrapper_disabled
  readonly gencc_disabled=$?
  is_preprocessor_run "$@"
  if [ "$?" == 1 ] || [ "$gencc_disabled" == 1 ]; then
    # shellcheck disable=SC2068
    $compiler $@
    return 0
  fi

  is_linking "$@"
  local -r linking=$?
  has_source "$@"
  local -r with_source=$?

  if [ "$linking" == 1 ] && [ "$with_source" == 1 ]; then
    parse_cmd_line "$@"
    main_compile "$@"
    if [ "$found_exe_file" == 1 ]; then
      ta_more_args+=" -o ${exe_file}"
    fi
    main_link "$ta_more_args" "${object_file}"
    if [ -f "${object_file}" ]; then
      rm "${object_file}"
    fi
  elif [ "$linking" == 1 ]; then
    main_link "$@"
  else
    parse_cmd_line "$@"
    main_compile "$@"
  fi
}

main_in "$@"
