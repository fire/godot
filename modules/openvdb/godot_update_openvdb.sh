rm -rf ../../thirdparty/openvdb
cd ../../thirdparty/
git clone https://github.com/AcademySoftwareFoundation/openvdb.git -b v6.1.0 openvdb
cd openvdb
rm -rf .circleci
rm -rf ci
rm -rf cmake
rm -rf openvdb_*
rm -rf .git
rm -rf tsc