#include <ISW/IswEvent.h>
#include <string.h>

typedef struct {
	int kind;
	int synthetic;
	uintptr_t target;
	uint32_t time;

	uint32_t key;
	uint32_t unicode;
	char text[8];
	uint16_t modifiers;
	int32_t x, y;
	int16_t root_x, root_y;
	int16_t shell_x, shell_y;

	uint8_t button;

	float scroll_delta_x, scroll_delta_y;
	int32_t scroll_discrete_x, scroll_discrete_y;
	uint8_t scroll_smooth;

	int notify_mode;
	int notify_detail;
	int focus_source;
	uint8_t same_screen;

	uint16_t redraw_width, redraw_height;
	uint16_t redraw_count;

	uint16_t geom_width, geom_height;
	uint16_t border_width;

	uint8_t to_root;
	uint8_t visibility;

	uint32_t protocol_type;
	uint8_t protocol_format;
	uint32_t protocol_data[5];
} IswEventFlat;

void isw_event_flatten(const IswEvent *e, IswEventFlat *out) {
	memset(out, 0, sizeof(*out));
	out->kind      = (int)e->kind;
	out->synthetic = e->any.synthetic;
	out->target    = (uintptr_t)e->any.target;
	out->time      = e->any.time;

	switch (e->kind) {
	case IswKeyDown:
	case IswKeyUp:
		out->key       = e->key.key;
		out->unicode   = e->key.unicode;
		memcpy(out->text, e->key.text, 8);
		out->modifiers = e->key.modifiers;
		out->x         = e->key.x;
		out->y         = e->key.y;
		out->root_x    = e->key.root_x;
		out->root_y    = e->key.root_y;
		out->shell_x   = e->key.shell_x;
		out->shell_y   = e->key.shell_y;
		break;
	case IswButtonDown:
	case IswButtonUp:
		out->button    = e->button.button;
		out->modifiers = e->button.modifiers;
		out->x         = e->button.x;
		out->y         = e->button.y;
		out->root_x    = e->button.root_x;
		out->root_y    = e->button.root_y;
		out->shell_x   = e->button.shell_x;
		out->shell_y   = e->button.shell_y;
		break;
	case IswScroll:
		out->modifiers = e->scroll.modifiers;
		out->x         = e->scroll.x;
		out->y         = e->scroll.y;
		out->root_x    = e->scroll.root_x;
		out->root_y    = e->scroll.root_y;
		out->shell_x   = e->scroll.shell_x;
		out->shell_y   = e->scroll.shell_y;
		out->scroll_delta_x   = e->scroll.delta_x;
		out->scroll_delta_y   = e->scroll.delta_y;
		out->scroll_discrete_x = e->scroll.discrete_x;
		out->scroll_discrete_y = e->scroll.discrete_y;
		out->scroll_smooth    = e->scroll.smooth;
		break;
	case IswMotion:
		out->modifiers = e->motion.modifiers;
		out->x         = e->motion.x;
		out->y         = e->motion.y;
		out->root_x    = e->motion.root_x;
		out->root_y    = e->motion.root_y;
		out->shell_x   = e->motion.shell_x;
		out->shell_y   = e->motion.shell_y;
		break;
	case IswEnter:
	case IswLeave:
		out->notify_mode   = (int)e->crossing.mode;
		out->notify_detail = (int)e->crossing.detail;
		out->modifiers     = e->crossing.modifiers;
		out->x             = e->crossing.x;
		out->y             = e->crossing.y;
		out->root_x        = e->crossing.root_x;
		out->root_y        = e->crossing.root_y;
		out->shell_x       = e->crossing.shell_x;
		out->shell_y       = e->crossing.shell_y;
		out->same_screen   = e->crossing.same_screen;
		break;
	case IswFocusIn:
	case IswFocusOut:
		out->notify_mode   = (int)e->focus.mode;
		out->notify_detail = (int)e->focus.detail;
		out->focus_source  = (int)e->focus.source;
		break;
	case IswRedraw:
		out->x            = e->redraw.x;
		out->y            = e->redraw.y;
		out->redraw_width = e->redraw.width;
		out->redraw_height = e->redraw.height;
		out->redraw_count = e->redraw.count;
		break;
	case IswGeometry:
		out->x            = e->geometry.x;
		out->y            = e->geometry.y;
		out->geom_width   = e->geometry.width;
		out->geom_height  = e->geometry.height;
		out->border_width = e->geometry.border_width;
		break;
	case IswReparent:
		out->x       = e->reparent.x;
		out->y       = e->reparent.y;
		out->to_root = e->reparent.to_root;
		break;
	case IswMap:
	case IswUnmap:
	case IswDestroy:
	case IswVisibility:
		out->visibility = e->structure.visibility;
		break;
	case IswProtocol:
	case IswWindowClose:
		out->protocol_type   = e->protocol.message_type;
		out->protocol_format = e->protocol.format;
		memcpy(out->protocol_data, e->protocol.data, sizeof(out->protocol_data));
		break;
	default:
		break;
	}
}
