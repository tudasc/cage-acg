echoerr() { echo "$@" 1>&2; }

color()(set -o pipefail;"$@" 2>&1>&3|sed $'s,.*,\e[31m&\e[m,'>&2)3>&1;

make clean
echoerr "Only Gencc"
CXX=/media/tim/Volume/Studium/Master/Semester3/MasterArbeit/Code/gencc/util/c_pp_wrapper_only_gencc.sh 
CC=/media/tim/Volume/Studium/Master/Semester3/MasterArbeit/Code/gencc/util/c_wrapper_only_gencc.sh 
if [[ "$1" == "MPI" ]]; then
  time OMPI_CC=$CC OMPI_CXX=$CXX make
else
  time make CC=$CC CXX=$CXX
fi
make clean
echoerr "All Opt"
CXX=/media/tim/Volume/Studium/Master/Semester3/MasterArbeit/Code/gencc/util/c_pp_wrapper_all_opt.sh 
CC=/media/tim/Volume/Studium/Master/Semester3/MasterArbeit/Code/gencc/util/c_wrapper_all_opt.sh
if [[ $1 == "MPI" ]]; then
  time OMPI_CC=$CC OMPI_CXX=$CXX make
else
  time make CC=$CC CXX=$CXX
fi
make clean
echoerr "No Opt"
CXX=/media/tim/Volume/Studium/Master/Semester3/MasterArbeit/Code/gencc/util/c_pp_wrapper_no_opt.sh 
CC=/media/tim/Volume/Studium/Master/Semester3/MasterArbeit/Code/gencc/util/c_wrapper_no_opt.sh
if [[ $1 == "MPI" ]]; then
  time OMPI_CC=$CC OMPI_CXX=$CXX make
else
  time make CC=$CC CXX=$CXX
fi
make clean
