#pragma once
#include "GameFramework/Actor.h"

/* Checks and returns. */
#define CHECK_DESTROY( LogCategory, Condition, Message, ...) if(Condition) { UE_LOG( LogCategory, Warning, TEXT(Message), ##__VA_ARGS__ ); Destroy(); return;}
#define CHECK_DESTROY_COMP( LogCategory, Condition, Message, ...) if(Condition) { UE_LOG( LogCategory, Warning, TEXT(Message), ##__VA_ARGS__ ); GetOwner()->Destroy(); return;}
#define CHECK_WARNING( LogCategory, Condition, Message, ...) if(Condition) { UE_LOG( LogCategory, Warning, TEXT(Message), ##__VA_ARGS__ ); }

/* Macro definitions for collision channels. */
#define ECC_Portal ECC_GameTraceChannel1
#define ECC_Interactable ECC_GameTraceChannel2
#define ECC_PortalBox ECC_GameTraceChannel3