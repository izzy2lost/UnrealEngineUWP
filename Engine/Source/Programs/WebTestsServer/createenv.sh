if [ -d "env" ]; then
	. ./env/bin/activate
else
	if [ "$OSTYPE" = "darwin" ]; then
		../../../Binaries/ThirdParty/Python3/Mac/bin/python3 -m venv env
	else
		../../../Binaries/ThirdParty/Python3/Linux/bin/python3 -m venv env
	fi
	. ./env/bin/activate
	python -m pip install --upgrade pip
	pip install -r requirements.txt
	local install_exit_code = $?
	if [ $install_exit_code -ne 0 ]; then
		rm -rf env
		exit $install_exit_code
	fi
fi
