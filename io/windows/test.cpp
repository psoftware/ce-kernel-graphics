#include "gr_object.h"
#include "gr_label.h"
#include "gr_button.h"
#include "gr_bitmap.h"

int main()
{
	gr_object *container = new gr_object(0,0,100,20,0);
	gr_button *button1 = new gr_button(0,0,25,15,1);
	gr_button *button2 = new gr_button(0,0,10,10,2);
	gr_button *button2a = new gr_button(10,0,10,15,2);
	gr_button *button3 = new gr_button(0,0,1,1,3);
	gr_bitmap *bitmap = new gr_bitmap(0,0,1,1,0);

	container->add_child(bitmap);
	container->add_child(button3);
	container->add_child(button2);
	container->add_child(button1);
	container->add_child(button2a);
	container->render();

	container->focus_child(button2);
	container->render();

	/*container->remove_child(button1);
	container->remove_child(button3);
	container->remove_child(button2);
	container->render();*/

	button2->render();
}