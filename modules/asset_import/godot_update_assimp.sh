rm -rf ../../thirdparty/assimp
cd ../../thirdparty/
git clone https://github.com/assimp/assimp.git
cd assimp
rm -rf .git
rm -rf cmake-modules
rm -rf doc
rm -rf packaging
rm -rf port
rm -rf samples
rm -rf scripts
rm -rf test
rm -rf tools
rm -rf contrib/zlib
rm -rf contrib/android-cmake
rm -rf contrib/gtest
rm .travis*
