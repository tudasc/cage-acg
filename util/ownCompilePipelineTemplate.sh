#Define the pass structure of O3, as lto pass manager is not yet capable of adding custom passes via opt level callback
O3_passes='annotation2metadata,cross-dso-cfi,globaldce,forceattrs,inferattrs,callsite-splitting,pgo-icall-prom,ipsccp,called-value-propagation,function-attrs,rpo-function-attrs,globalsplit,wholeprogramdevirt,globalopt,mem2reg,constmerge,deadargelim,aggressive-instcombine,instcombine,inliner-wrapper,globalopt,globaldce,argpromotion,instcombine,jump-threading,sroa,tailcallelim,function-attrs,require<globals-aa>,function(invalidate<aa>),loop-simplify,lcssa,gvn,memcpyopt,dse,mldst-motion,loop-simplify,lcssa,loop-distribute,loop-vectorize,loop-unroll,transform-warning,instcombine,simplifycfg,sccp,instcombine,bdce,slp-vectorizer,vector-combine,alignment-from-assumptions,instcombine,jump-threading,lowertypetests,lowertypetests,simplifycfg,elim-avail-extern,globaldce,annotation-remarks'

#Same as O3_passes, but with genCC pass at the end
O3_with_genCC_passes='annotation2metadata,cross-dso-cfi,globaldce,forceattrs,inferattrs,callsite-splitting,pgo-icall-prom,ipsccp,called-value-propagation,function-attrs,rpo-function-attrs,globalsplit,wholeprogramdevirt,globalopt,mem2reg,constmerge,deadargelim,aggressive-instcombine,instcombine,inliner-wrapper,globalopt,globaldce,argpromotion,instcombine,jump-threading,sroa,tailcallelim,function-attrs,require<globals-aa>,function(invalidate<aa>),loop-simplify,lcssa,gvn,memcpyopt,dse,mldst-motion,loop-simplify,lcssa,loop-distribute,loop-vectorize,loop-unroll,transform-warning,instcombine,simplifycfg,sccp,instcombine,bdce,slp-vectorizer,vector-combine,alignment-from-assumptions,instcombine,jump-threading,lowertypetests,lowertypetests,simplifycfg,elim-avail-extern,globaldce,annotation-remarks,genCC'

libgenCC_path=@libgenCC_path@
runtimeComponent=@runtimeComponent@
runtimeComponent_path=$(dirname "${runtimeComponent}")
pluginCapableLLD=@pluginCapableLLD@

export LD_LIBRARY_PATH="$LD_LIBRARY_PATH":@runtimeComponent_path@

clang++-13 -S -O0 -Xclang -disable-O0-optnone -flto=full -emit-llvm @SOURCES@

#do optimization seperately with opt for better control of pipeline,
#this step can be included in clang-13 call
#output must be bitcode, as we can not link ir code
for IRFile in *.s; do
    opt-13 -O3 $IRFile -o=${IRFile%.*}.bc
done


#use lld with plugin support, load plugin and run all passes
#options -plugin-opt=O3 and --lto-O3 are redundant
#this step can not be included in the clang-13 call via -Wl, as it mangles the textual pipeline descriptions

$pluginCapableLLD --hash-style=both --build-id --eh-frame-hdr -m elf_x86_64 -dynamic-linker /lib64/ld-linux-x86-64.so.2 -o a.out /lib/x86_64-linux-gnu/crt1.o /lib/x86_64-linux-gnu/crti.o /usr/bin/../lib/gcc/x86_64-linux-gnu/10/crtbegin.o -L/usr/bin/../lib/gcc/x86_64-linux-gnu/10 -L/usr/bin/../lib/gcc/x86_64-linux-gnu/10/../../../../lib64 -L/lib/x86_64-linux-gnu -L/lib/../lib64 -L/usr/lib/x86_64-linux-gnu -L/usr/lib/../lib64 -L/usr/lib/llvm-13/bin/../lib -L/lib -L/usr/lib -plugin /usr/lib/llvm-13/bin/../lib/LLVMgold.so -plugin-opt=mcpu=x86-64 -plugin-opt=O3 -save-temps --lto-O3 --lto-load-pass-plugin=$libgenCC_path --lto-newpm-passes=$O3_with_genCC_passes *.bc $runtimeComponent -lstdc++ -lm -lgcc_s -lgcc -lc -lgcc_s -lgcc /usr/bin/../lib/gcc/x86_64-linux-gnu/10/crtend.o /lib/x86_64-linux-gnu/crtn.o
#as we use save temps, we can disassemble (bc->ir) them
llvm-dis-13 *.bc
rm *.bc *.o *.s