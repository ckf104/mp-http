cd vnetUtils
dos2unix ./examples/* ./helper/*
chmod +x ./examples/makeVNet ./examples/removeVNet ./helper/*
cd ./examples
sudo bash ./makeVNet < topo.txt
cd ../../