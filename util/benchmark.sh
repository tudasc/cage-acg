git clone git@git.rwth-aachen.de:tuda-sc/projects/metacg.git

cd metacg
mkdir build
mkdir install
cd build
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_INSTALL_PREFIX=../install ..
make install -j

cd ../..

git clone git@git.rwth-aachen.de:tim.heldmann/gencc.git
cd gencc
git switch llvm18
mkdir build
cd build
cmake -Dmetacg_DIR=../../metacg/install/lib/cmake/metacg -Dspdlog_DIR=../../metacg/install/lib/cmake/spdlog -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
make -j