// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Runtime/Online/OnlineSubsystemUtils/Classes/OnlineBeaconHost.h"
#include "PartyBeaconHost.generated.h"

/** The result code that will be returned during party reservation */
UENUM()
namespace EPartyReservationResult
{
	enum Type
	{
		// Pending request due to async operation
		RequestPending,
		// An unknown error happened
		GeneralError,
		// All available reservations are booked
		PartyLimitReached,
		// Wrong number of players to join the session
		IncorrectPlayerCount,
		// No response from the host
		RequestTimedOut,
		// Already have a reservation entry for the requesting party leader
		ReservationDuplicate,
		// Couldn't find the party leader specified for a reservation update request 
		ReservationNotFound,
		// Space was available and it's time to join
		ReservationAccepted,
		// The beacon is paused and not accepting new connections
		ReservationDenied,
		// This player is banned
		ReservationDenied_Banned,
		// The reservation request was canceled before being sent
		ReservationRequestCanceled
	};
}

namespace EPartyReservationResult
{
	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EPartyReservationResult::Type SessionType)
	{
		switch (SessionType)
		{
		case RequestPending:
			{
				return TEXT("Pending Request");
			}
		case GeneralError:
			{
				return TEXT("General Error");
			}
		case PartyLimitReached:
			{
				return TEXT("Party Limit Reached");
			}
		case IncorrectPlayerCount:
			{
				return TEXT("Incorrect Player Count");
			}
		case RequestTimedOut:
			{
				return TEXT("Request Timed Out");
			}
		case ReservationDuplicate:
			{
				return TEXT("Reservation Duplicate");
			}
		case ReservationNotFound:
			{
				return TEXT("Reservation Not Found");
			}
		case ReservationAccepted:
			{
				return TEXT("Reservation Accepted");
			}
		case ReservationDenied:
			{
				return TEXT("Reservation Denied");
			}
		case ReservationDenied_Banned:
			{
				return TEXT("Reservation Banned");
			}
		case ReservationRequestCanceled:
			{
				return TEXT("Request Canceled");
			}
		}
		return TEXT("");
	}
}

/** A single player reservation */
USTRUCT()
struct FPlayerReservation
{
	GENERATED_USTRUCT_BODY()
	
	#if CPP
	FPlayerReservation() :
		ElapsedTime(0.0f)
	{}
	#endif

	/** Unique id for this reservation */
	UPROPERTY(Transient)
	FUniqueNetIdRepl UniqueId;

	/** Info needed to validate user credentials when joining a server */
	UPROPERTY(Transient)
	FString ValidationStr;

	/** Elapsed time since player made reservation and was last seen */
	UPROPERTY(Transient)
	float ElapsedTime;
};

/** A whole party reservation */
USTRUCT()
struct FPartyReservation
{
	GENERATED_USTRUCT_BODY()

	/** Team assigned to this party */
	UPROPERTY(Transient)
	int32 TeamNum;

	/** Player initiating the request */
	UPROPERTY(Transient)
	FUniqueNetIdRepl PartyLeader;

	/** All party members (including party leader) in the reservation */
	UPROPERTY(Transient)
	TArray<FPlayerReservation> PartyMembers;
};

/**
 * Delegate fired when a the beacon host detects a reservation addition/removal
 */
DECLARE_DELEGATE(FOnReservationChanged);

/**
 * Delegate fired when a the beacon host has been told to cancel a reservation
 *
 * @param PartyLeader leader of the canceled reservation
 */
DECLARE_DELEGATE_OneParam(FOnCancelationReceived, const class FUniqueNetId&);

/**
 * Delegate called when the beacon gets any request, allowing the owner to validate players at a higher level (bans,etc)
 * 
 * @param PartyMembers players making up the reservation
 * @return true if these players are ok to join
 */
DECLARE_DELEGATE_RetVal_OneParam(bool, FOnValidatePlayers, const TArray<FPlayerReservation>&);

/**
 * Delegate fired when a the beacon host detects a duplicate reservation
 *
 * @param PartyReservation reservation that is found to be duplicated
 */
DECLARE_DELEGATE_OneParam(FOnDuplicateReservation, const FPartyReservation&);

/**
 * Delegate fired when a the beacon host detects that all reservations are full
 */
DECLARE_DELEGATE(FOnReservationsFull);

/**
 * A beacon host used for taking reservations for an existing game session
 */
UCLASS(transient, notplaceable, config=Engine)
class ONLINESUBSYSTEMUTILS_API APartyBeaconHost : public AOnlineBeaconHost
{
	GENERATED_UCLASS_BODY()

	// Begin AActor Interface
	virtual void Tick(float DeltaTime) OVERRIDE;
	// End AActor Interface

	// Begin AOnlineBeaconHost Interface 
	virtual bool InitHost() OVERRIDE;
	virtual AOnlineBeaconClient* SpawnBeaconActor() OVERRIDE;
	// End AOnlineBeaconHost Interface 

	/**
	 * Initialize the party host beacon
	 *
	 * @param InTeamCount number of teams to make room for
	 * @param InTeamSize size of each team
	 * @param InMaxReservation max number of reservations allowed
	 * @param InSessionName name of session related to the beacon
	 *
	 * @return true if successful created, false otherwise
	 */
	virtual bool InitHostBeacon(int32 InTeamCount, int32 InTeamSize, int32 InMaxReservations, FName InSessionName, int32 InForceTeamNum = 0);

	/** 
	 * Reconfigures the beacon for a different team/player count configuration
	 * Allows dedicated server to change beacon parameters after a playlist configuration has been made
	 * Does no real checking against current reservations because we assume the UI wouldn't let 
	 * this party start a gametype if they were too big to fit on a team together
	 * @param InNumTeams the number of teams that are expected to join
	 * @param InNumPlayersPerTeam the number of players that are allowed to be on each team
	 * @param InNumReservations the total number of players to allow to join (if different than team * players)
	 */
	virtual bool ReconfigureTeamAndPlayerCount(int32 InNumTeams, int32 InNumPlayersPerTeam, int32 InNumReservations);

	/**
	 * Notify the beacon of a player logout
	 *
	 * @param PlayerId UniqueId of player logging out
	 */
	virtual void HandlePlayerLogout(const FUniqueNetIdRepl& PlayerId);

	/**
	 * Get the current reservation count inside the beacon
	 *
	 * @return number of consumed reservations
	 */
	virtual int32 GetReservationCount() const { return Reservations.Num(); }

	/**
	 * Get the number of players on a team across all existing reservations
	 *
	 * @param TeamIdx team to query
	 *
	 * @return number of players on the given team
	 */
	int32 GetNumPlayersOnTeam(int32 TeamIdx) const;

	/**
	 * Finds the current team assignment of the given player net id.
	 * Looks at the reservation entries to find the player.
	 *
	 * @param PlayerId net id of player to find
	 * @return index of team assignment for the given player, INDEX_NONE if not found
	 */
	int32 GetTeamForCurrentPlayer(const FUniqueNetId& PlayerId);

	/**
	 * Get the number of teams.
	 *
	 * @return The number of teams.
	 */
	int32 GetNumTeams() const { return NumTeams; }

	/**
	 * Does a given player id have an existing reservation
     *
	 * @param PlayerId uniqueid of the player to check
     *
	 * @return true if a reservation exists, false otherwise
	 */
	virtual bool PlayerHasReservation(const FUniqueNetId& PlayerId) const;

	/**
	 * Obtain player validation string from party reservation entry
	 *
	 * @param PlayerId unique id of player to find validation in an existing reservation
	 * @param OutValidation [out] validation string used when player requested a reservation
	 *
	 * @return true if reservation exists for player
	 *
	 */
	virtual bool GetPlayerValidation(const FUniqueNetId& PlayerId, FString& OutValidation) const;

	/**
	 * Attempts to add a party reservation to the beacon
     *
     * @param ReservationRequest reservation attempt
     *
     * @return add attempt result
	 */
	EPartyReservationResult::Type AddPartyReservation(const FPartyReservation& ReservationRequest);

	/**
	 * Attempts to remove a party reservation from the beacon
     *
     * @param PartyLeader reservation leader
	 */
	virtual void RemovePartyReservation(const FUniqueNetIdRepl& PartyLeader);

	/**
	 * Handle a reservation request received from an incoming client
	 *
	 * @param Client client beacon making the request
	 * @param ReservationRequest payload of request
	 */
	virtual void ProcessReservationRequest(class APartyBeaconClient* Client, const FString& SessionId, const FPartyReservation& ReservationRequest);

	/**
	 * Handle a reservation cancelation request received from an incoming client
	 *
	 * @param Client client beacon making the request
	 * @param PartyLeader reservation leader
	 */
	virtual void ProcessCancelReservationRequest(class APartyBeaconClient* Client, const FUniqueNetIdRepl& PartyLeader);

	/**
	 * Delegate triggered when a new client connection is made
	 *
	 * @param NewClientActor new client beacon actor
	 * @param ClientConnection new connection established
	 */
	virtual void ClientConnected(class AOnlineBeaconClient* NewClientActor, class UNetConnection* ClientConnection);

	/**
	 * Delegate fired when a the beacon host detects a reservation addition/removal
	 */
	FOnReservationsFull& OnReservationsFull() { return ReservationsFull; }

	/**
	 * Delegate fired when a the beacon host detects that all reservations are full
	 */
	FOnReservationChanged& OnReservationChanged() { return ReservationChanged; }

	/**
	 * Delegate fired when a the beacon host cancels a reservation
	 */
	FOnCancelationReceived& OnCancelationReceived() { return CancelationReceived; }

	/**
	 * Delegate fired when a the beacon detects a duplicate reservation
	 */
	FOnDuplicateReservation& OnDuplicateReservation() { return DuplicateReservation; }

	/**
	 * Delegate called when the beacon gets any request, allowing the owner to validate players at a higher level (bans,etc)
	 */
	FOnValidatePlayers& OnValidatePlayers() { return ValidatePlayers; }

	/**
	 * Output current state of reservations to log
	 */
	virtual void DumpReservations() const;

protected:

	/** Session tied to the beacon */
	UPROPERTY(Transient)
	FName SessionName;
	/** Number of currently consumed reservations */
	UPROPERTY(Transient)
	int32 NumConsumedReservations;
	/** Maximum allowed reservations */
	UPROPERTY(Transient)
	int32 MaxReservations;
	/** Number of teams in the game */
	UPROPERTY(Transient)
	int32 NumTeams;
	/** Number of players on each team for balancing */
	UPROPERTY(Transient)
	int32 NumPlayersPerTeam;
	/** Team that the host has been assigned to */
	UPROPERTY(Transient)
	int32 ReservedHostTeamNum;
	/** Team that everyone is forced to in single team games */
	UPROPERTY(Transient)
	int32 ForceTeamNum;

	/** Delegate fired when the beacon indicates all reservations are taken */
	FOnReservationsFull ReservationsFull;
	/** Delegate fired when the beacon indicates a reservation add/remove */
	FOnReservationChanged ReservationChanged;
	/** Delegate fired when the beacon indicates a reservation cancelation */
	FOnCancelationReceived CancelationReceived;
	/** Delegate fired when the beacon detects a duplicate reservation */
	FOnDuplicateReservation DuplicateReservation;
	/** Delegate fired when asking the beacon owner if this reservation is legit */
	FOnValidatePlayers ValidatePlayers;

	/** Current reservations in the system */
	UPROPERTY(Transient)
	TArray<FPartyReservation> Reservations;

	/** Seconds that can elapse before a reservation is removed due to player not being registered with the session */
	UPROPERTY(Transient, Config)
	float SessionTimeoutSecs;
	/** Seconds that can elapse before a reservation is removed due to player not being registered with the session during a travel */
	UPROPERTY(Transient, Config)
	float TravelSessionTimeoutSecs;

	/** Players that are expected to join shortly */
	TArray< TSharedPtr<FUniqueNetId> > PlayersPendingJoin;

	/**
	 * Handle a newly added player
	 *
	 * @param NewPlayer reservation of newly joining player
	 */
	void NewPlayerAdded(const FPlayerReservation& NewPlayer);

	/**
	 * Get an existing reservation for a given party
	 * 
	 * @param PartyLeader UniqueId of party leader for a reservation
	 *
	 * @return index of reservation, INDEX_NONE otherwise
	 */
	int32 GetExistingReservation(const FUniqueNetIdRepl& PartyLeader);

	/**
	 * Initializes the team array so that random choices can be made from it
	 * Also initializes the host's team number (random from range)
	 */
	void InitTeamArray();

	/** 
	 * Determine if there are any teams that can fit the current party request.
	 * 
	 * @param PartySize number of players in the party making a reservation request
	 * @return true if there are teams available, false otherwise 
	 */
	bool AreTeamsAvailable(int32 PartySize) const;

	/**
	 * Determine the team number for the given party reservation request.
	 * Uses the list of current reservations to determine what teams have open slots.
	 *
	 * @param PartyRequest the party reservation request received from the client beacon
	 * @return index of the team to assign to all members of this party
	 */
	int32 GetTeamAssignment(const FPartyReservation& Party);

	/**
	 * Does the session match the one associated with this beacon
	 *
	 * @param SessionId session to compare
	 *
	 * @return true if the session matches, false otherwise
	 */
	bool DoesSessionMatch(const FString& SessionId) const;
};
