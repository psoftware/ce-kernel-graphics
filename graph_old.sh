#!/bin/bash

./run | grep "GRAPH-" > loggraph.txt

LISTGRAP=`cat loggraph.txt | grep GRAPH`;
PROCLIST=`echo "$LISTGRAP" | cut -d'*' -f2 | sort -n | uniq`;
FIRSTTIME=`echo "$LISTGRAP" | cut -d'[' -f2 | cut -d']' -f1 | sort -n | head -n 1`
LASTTIME=`echo "$LISTGRAP" | cut -d'[' -f2 | cut -d']' -f1 | sort -n | tail -n 1`
#echo "$FIRSTTIME $LASTTIME";

#per ogni processo
while read -r procid; do
	ACTIVE=0;
	STRRES=`echo -e "p${procid}\t"`;
	for i in `seq $FIRSTTIME $LASTTIME`; do
		WHOALL=`cat loggraph.txt | grep "GRAPH"`;
		WHO=`echo "$WHOALL" | grep "GRAPH-S" | grep "\[$i\]" | cut -d'*' -f2`;
		WHOME=`echo "$WHO" | grep "$procid"`;
		WHOTERM=`echo "$WHOALL" | grep "GRAPH-T" | grep "\[$i\]" | cut -d'*' -f2`;
		#echo $WHO;
		if [ "$WHOTERM" == "$procid" ]; then
			STRRES=`echo -e "${STRRES}\e[31mT\e[0m"`
			ACTIVE=0;
		elif [ "$WHO" == "" ]; then
			if [ "$ACTIVE" -eq "1" ]; then
				STRRES="${STRRES}="
			else
				STRRES="${STRRES}_"
			fi
               	elif [ "$WHOME" == "$procid" ]; then
			#echo "found procid $procid on $i"
			ACTIVE=1;
			STRRES="${STRRES}|"
		else
			if [ "$ACTIVE" -eq "1" ]; then
				STRRES="${STRRES}|"
				ACTIVE=0;
			else
				STRRES="${STRRES}_"
			fi
            	fi
	done
	echo "$STRRES"
done <<< "$PROCLIST"
	#per ogni unità di tempo
rm loggraph.txt
