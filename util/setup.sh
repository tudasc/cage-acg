git clone git@git.rwth-aachen.de:tuda-sc/projects/metacg.git

cd metacg
mkdir build
mkdir install
cd build
#This needs CMake 3.24 or newer (currently not enforced)
#It this fails on can delete the policy command and reconfigure
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_INSTALL_PREFIX=../install ..
make install -j

cd ../..

git clone git@git.rwth-aachen.de:tim.heldmann/gencc.git
cd gencc
git switch llvm18
mkdir build
cd build
#This needs LLVM 18
cmake -Dmetacg_DIR=../../metacg/install/lib/cmake/metacg -Dspdlog_DIR=../../metacg/install/lib/cmake/spdlog -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
make -j