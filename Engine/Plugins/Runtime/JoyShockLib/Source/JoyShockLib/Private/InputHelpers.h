
// EmmettJnr: Updated for UE5

#pragma once

class JoyShock;

bool handle_input(JoyShock* jc, uint8_t* packet, int len, bool& hasIMU);
