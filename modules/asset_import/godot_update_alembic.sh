rm -rf ../../thirdparty/alembic
cd ../../thirdparty/
git clone https://github.com/alembic/alembic.git -b 1.7.10 alembic
cd alembic
rm -rf maya
rm -rf prman
rm -rf python
rm -rf arnold
rm -rf houdini
rm -rf examples
rm -rf bin
rm -rf .git
rm -rf cmake
rm -rf lib/python