#rm -rf ../../thirdparty/tbb
cd ../../thirdparty/
#git clone https://github.com/01org/tbb.git -b 2019_U5
cd tbb
rm -rf .git
rm -rf cmake
rm -rf examples
rm -rf python
rm -f .gitignore
rm -f .gitattributes