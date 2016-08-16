while inotifywait -e close_write topo.dot; do
	cat topo.dot;
	dot -Tpng topo.dot -O;
done
