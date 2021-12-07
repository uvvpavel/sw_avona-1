
while true
do
	./ww_demo

	if [ $? -ne 0 ]; then
		break
	fi

	aplay -c2 -r16000 -fS16_LE  audio.dat
done

