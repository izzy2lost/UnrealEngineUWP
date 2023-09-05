if [ -d "env" ]; then
	source ./env/Scripts/activate.sh
else
	run ../../../Binaries/ThirdParty/Python3/Mac/python3 -m venv env
	source ./env/Scripts/activate.sh
	run python -m  pip install --upgrade pip
	run pip install -r requirements.txt
	if [ $? -ne 0 ]; then
		rm -rf env
		exit $?
	fi
fi
