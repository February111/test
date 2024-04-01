if [ "../lib/libStrategyPlatform.a" -nt "./StrategyPlatform" ]; then
	make -C ../src clean
	make -C ../src
else
	echo ".a file is not newer than exe file"

fi
