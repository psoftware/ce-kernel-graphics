#include "gr_object.h"
#include "gr_label.h"
#include "gr_button.h"

int main()
{
	gr_object *container = new gr_object(0,0,0,0,0);
	gr_button *button1 = new gr_button(0,0,0,0,1);
	gr_button *button2 = new gr_button(0,0,0,0,2);
	gr_button *button3 = new gr_button(0,0,0,0,3);

	container->add_child(button3);
	container->add_child(button2);
	container->add_child(button1);
	container->render();

	container->remove_child(button1);
	container->remove_child(button3);
	container->remove_child(button2);
	container->render();

	button2->render();
}