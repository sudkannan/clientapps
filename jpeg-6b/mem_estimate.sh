
rm memstat.out

for i in {1..100}
do
	cat /proc/meminfo | grep -r "ctive" >>memstat.out
	sleep 1
done
