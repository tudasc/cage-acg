#!/bin/bash

O3_passes='annotation2metadata,cross-dso-cfi,globaldce,forceattrs,inferattrs,callsite-splitting,pgo-icall-prom,ipsccp,called-value-propagation,function-attrs,rpo-function-attrs,globalsplit,wholeprogramdevirt,globalopt,mem2reg,constmerge,deadargelim,aggressive-instcombine,instcombine,inliner-wrapper,globalopt,globaldce,argpromotion,instcombine,jump-threading,sroa,tailcallelim,function-attrs,require<globals-aa>,function(invalidate<aa>),loop-simplify,lcssa,gvn,memcpyopt,dse,mldst-motion,loop-simplify,lcssa,loop-distribute,loop-vectorize,loop-unroll,transform-warning,instcombine,simplifycfg,sccp,instcombine,bdce,slp-vectorizer,vector-combine,alignment-from-assumptions,instcombine,jump-threading,lowertypetests,lowertypetests,simplifycfg,elim-avail-extern,globaldce,annotation-remarks'
O3_with_genCC_passes=$O3_passes",genCC"
libgenCC=/media/tim/Volume/Studium/Master/Semester3/MasterArbeit/Code/gencc/cmake-build-debug/lib/libplugin.so
runtimeComponent=/media/tim/Volume/Studium/Master/Semester3/MasterArbeit/Code/gencc/cmake-build-debug/lib/Runtime/libgenCCRT.so
runtimeComponent_path=$(dirname "${runtimeComponent}")

echoerr() { echo "$@" 1>&2; }

function get_clang(){
  readonly clang_pp=clang++-13
  readonly clang=clang-13
}

function is_cpp() {
  for arg in "$@"; do
    local extension_of_arg="${arg##*.}"
    case "$extension_of_arg" in  cpp | cxx | cc )
      return 1
    ;;
    esac
  done
  return 0
}

function get_first_compile(){
  local counter=0
  for line in $@; do
    if [[ $line == " \"/usr/lib/llvm"*   ]]; then
      return $counter
    fi
     counter=$(($counter+1))
  done 
echo "Could not detect compile statement!"
return $counter
}


function get_first_link(){
  local counter=0
  for line in $@; do
    if [[ $line == " \"/usr/bin/ld"*   ]]; then
      return $counter
    fi
     counter=$(($counter+1))
  done 
echo "Could not detect link statement!"
return $counter
}

function split_pipeline(){
  IFS=$'\n'
  local -a clang_output=($@)
  
  get_first_compile ${clang_output[@]}
  compile_statements_begin=$?
  #echo "csb:$compile_statements_begin"
  
  get_first_link ${clang_output[@]}
  link_statements_begin=$?
  #echo "lsb:$link_statements_begin"
  
  compile_statements_end=$(($link_statements_begin))
  #echo "cse:$compile_statements_end"
  compile_statements_num=$(($compile_statements_end-$compile_statements_begin))
  #echo "csn:$compile_statements_num"
  

  link_statements_end=${#clang_output[@]}
  #echo "lse:$link_statements_end"
  link_statements_num=$(($link_statements_end-$link_statements_begin))
  #echo "lsn:$link_statements_num"
  
  clang_compile_invoke=("${clang_output[@]:$compile_statements_begin:$compile_statements_num}")
  clang_link_invoke=("${clang_output[@]:$link_statements_begin:$link_statements_num}")
}

function filter_options(){
  for arg in "$@"; do
    case "$arg" in -\#\#\# )
      return 1
    ;;
    esac
    case "$arg" in -flto)
      echo "This wrapper is not designed to do lto"
      exit 1  
    ;;
    esac
  done
}

function main_in(){
  get_clang

  filter_options $@
  echo_only=$?
 
  #is_cpp "$@";
  #was_cpp=$?
  #if [[ $was_cpp -eq 1 ]]; then
  #  #this might not work if stderr and stdout are both used
  #  echo "Using C++ compiler"
  #  compile_pipeline="$($clang_pp -### -flto "$@" 2>&1)"
  #else  
  #  echo "Using C compiler"
  #  compile_pipeline="$($clang -### -flto "$@" 2>&1)"
  #fi
  
  #echo "Using C++ compiler"
  compile_pipeline="$($clang_pp -### -flto "$@" 2>&1)"
    
  split_pipeline "$compile_pipeline"
  
  for single_compile in ${clang_compile_invoke[@]}; do
    if [[ $echo_only -eq 1 ]]; then
      echo "Echo Compile:$single_compile"
    else
      #echo "Eval Compile:$single_compile"
      echoerr "Compile"
      eval "time $single_compile"
    fi  
  done

  for single_link in ${clang_link_invoke[@]}; do
    IFS=' '
    split_link=($single_link)
    split_link[0]=/media/tim/Volume/Studium/Master/Semester3/MasterArbeit/LLVMRepo/llvm-project/lld/cmake-build-debug/bin/ld.lld
    split_link+=("-L${runtimeComponent_path} -rpath ${runtimeComponent_path} -lgenCCRT --lto-load-pass-plugin=$libgenCC --lto-newpm-passes='$O3_with_genCC_passes'")
    #"-L${runtimeComponent_path} -lgenCCRT --lto-load-pass-plugin=$libgenCC --lto-newpm-passes=$O3_with_genCC_passes "
    if [[ $echo_only -eq 1 ]]; then
      echo "Echo Link: ${split_link[@]}"
    else
      #echo "Eval Link: ${split_link[@]}"
      echoerr "Link"
      eval "time ${split_link[@]}"
    fi  
  done
  
}


echo "-------------------------------------------"
echo "         Gencc wrapper got called"
echo "-------------------------------------------"

main_in "$@"
