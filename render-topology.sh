cat topo.dot;
dot -Tpng topo.dot -o topo.png;

xdg-open topo.png&

while inotifywait -e close_write topo.dot; do
	cat topo.dot;
	dot -Tpng topo.dot -o topo.png;
done
