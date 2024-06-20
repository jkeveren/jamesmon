#pragma once

#include <string>

namespace pgm {
	// Error handling strategy.
	// This is a lot of comments. Just the way I like it. Explain every decision so it sticks.
	// Pass error by reference to all functions that can error in order to remove the nead to try-catch every errorable function call just to add more error information at each level.
	// Check whether an error occured by (implicitly) casting to bool.
	// The goal is for a deep error caused by a high level mistake to be easy to debug just by seeing the error message. All relavent variable values should be in the error messages.
	// However deep errors should not be programmatically accessible; only the highest level error should be. This allows the implementation of the unit to be changed without risk of creating breaking changes.
	// This error handling strategy has overhead because of all the string building so don't use this for things that you expect to error a lot, or at least just use a const string.
	// If this is used in a performace critical application that is expected to error a lot then this is not ideal.
	// If it's performance critical and simple you can re-implement without this; if it's complex then string building is probably not that much of an overhead compared to the copmlex task.
	class error {
		private:
		// message_stack is a string of error messages seperated by newlines.
		// Each message is appended in chronological order. This results in the lowest level messages being at the top of the stack and are the first ones printed.
		// Each message should be verbose and contain lots of information that could be useful in debugging the issue like labeled variable values.
		// message_stack is private because deep errors should not be machine readable.
		// Machine readable errors become part of a units external API.
		// This would cause low level changes to the unit to effect the external API which makes them breaking changes.
		// If a user of a unit needs low level error access then the user should implement it and/or the unit should provide a way of injecting low level resources.
		std::string message_stack;

		public:
		// reason is a machine readable version of the latest error appended to message_stack.
		// This does not stack like message_stack for the same reasons message_stack is private.
		// It only stores the highest level reason for the error.
		int reason;

		// Common reasons are defined here.
		// When defining your own reasons, make sure to start the enum at custom_reason_start!
		// Using a regular enum instead of enum class for implicit conversion to int.
		enum reasons {
			reason_none = 0, // None means no error occurred.
			reason_other = 1, // Other should be used when an error occured but you don't want to make the machine readable reason part of the external API. Mostly for implementation details that can change.
			// reason_none and reason_other being 0 and 1 makes them also work as sensible exit codes so you can just exit with the error reason which defaults to other which is 1. Not a great strat if you want unique exit codes for different circumstances.
			custom_reason_start, // Custom reasons should start at this int to avoid colliding with the previous reasons.
		};
		
		// Default constructor.
		// Constructs an empty error. Indicates that an error has not occurred.
		error();

		// Constructs an error with message and reason. Indicates that an error has occurred.
		error(const std::string &message, int reason = reason_other);

		// Returns true if an error has occurred.
		operator bool() const;

		// Appends the highest level error message to the message stack and overrides the reason.
		// Returns reference to *this for chaining.
		error &
		append(const std::string &message, int reason = reason_other);

		// Prints the message_stack and returns reason int (handy for exiting program e.g. "int main() {...; return something.error().append("OOPSIE WOOPSIE!! Uwu We made a fucky wucky!!").print();}").
		// message_stack is only accessible through printing to prevent easy access to the calling unit for the reasons stated above in the section about message_stack etc.
		int
		print() const;

		// Appends message from errno in a thread safe way (using strerror_r instead of strerror).
		error
		strerror();
	};
}
