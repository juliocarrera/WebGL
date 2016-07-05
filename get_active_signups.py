#!/usr/bin/python
import sys
import re
import urllib
import urllib2
import json



# Signupgenius url
SIGNUPGENIUS_URL = 'https://api.signupgenius.com/v2/k/'

# sign ups created active
CREATED_ACTIVE = 'signups/created/active/?'
GROUP_MEMBERS = 'groups/groupid/members/?'
SIGNUP_REPORT = 'signups/report/filled/signupid/?'
SIGNUP_AVAILABLE = 'signups/available/signupid/?'

# signupgenius API key
API_KEY = 'N1B5T29Bd0lSRWpLSXREbWY3WjJmQT09'

# test event ids for testing only 
test_event_l = [ ( '8045148', 'API Testing' ), 
                 ( '7707531', 'Broomstones Octoberfest Open Spiel - Team Sign Up' ) ]


# for providing an API interface to get signup data
BROOMSTONES_SERVER = 'julio'
BROOMSTONES_SERVER_PATH = '/home/brooms6/www/{0}/broomstones3/'.format( BROOMSTONES_SERVER )
SIGNUP_SUMMARY_PATH = '/home/brooms6/www/{0}/broomstones3/sug_events/'.format( BROOMSTONES_SERVER )


# returns the json data for the given signupgenius endpoint request
def get_signupgenius_request( endpoint_s, groupid_s = None, eventid_s = None ):
    '''
    returns the JSON data corresponding to the given signupgenius request
    this function is called by all functions requesting event data via the SUG API endpoints

    signupgenius query example
    https://api.signupgenius.com/v2/k/user/profile/?user_key=ABX12ZYX447
    '''
    args = { 'user_key': API_KEY
             }

    # urlencode the POST data
    q = urllib.urlencode( args )

    headers = { 'Accept' : 'application/json' }

    # build the request string
    if endpoint_s == SIGNUP_REPORT:
        if eventid_s == None:
            print "Error - a signup id must be provided"
            return False

        # print "signup_id_s: " + eventid_s
        request_s = endpoint_s.replace( 'signupid', eventid_s )

    elif endpoint_s == SIGNUP_AVAILABLE:
        if eventid_s == None:
            print "Error - a signup id must be provided"
            return False

        # print "signup_id_s: " + eventid_s
        request_s = endpoint_s.replace( 'signupid', eventid_s )

    elif endpoint_s == GROUP_MEMBERS:
        if groupid_s == None:
            print "Error - a group id must be provided"
            return False

        request_s = endpoint_s.replace( 'groupid', groupid_s )

    else:
        # the CREATED_ACTIVE endpoint doesn't need any arguments
        request_s = endpoint_s


    req = urllib2.Request( SIGNUPGENIUS_URL + request_s + q, None, headers )
    # print "signupgenius query: {0}".format( req.get_full_url() )
    # search_results = urllib.urlopen( SIGNUPGENIUS_URL + endpoint_s )

    try:
        response = urllib2.urlopen( req )
    except urllib2.HTTPError as err:
        print "signupgenius returned HTTP error: {0}".format( err )
        return False

    try:
        data = json.loads( response.read() )
    except AttributeError as err:
        print "JSON load error: {0}".format( err )

    response.close()

    return data
# end get_signupgenius_request


def get_active_events():
    '''
    returns the list of all active events
    '''
    # return the test ids to avoid too many calls to the API
    results = []

    # get the response
    response = get_signupgenius_request( CREATED_ACTIVE )
    if response == False:
        return False
    # print search_results.headers

    try: 
        success = response[ 'success' ]
    except KeyError:
        # we didn't get the expected response so print it out
        print "signupgenius query failed"
        print json.dumps( response, sort_keys = True, indent = 4, separators = (',', ': ') )  
        return results

    if success == False:
        print 'signupgenius returned a failure\n'
        print json.dumps( response, sort_keys = True, indent = 4, separators = (',', ': ') )  
        return results

    for event in response[ 'data' ]:
        # print 'found event id: {0} title: {1}'.format( event[ 'id' ], event[ 'title' ] )
        results.append( ( str( event[ 'id' ] ), event[ 'title' ] ) )

    return results
# end get_active_events


def get_signed_up_members( event ):
    '''
    returns the list of members signed up for the given event
    '''
    signups = []

    # get the response
    response = get_signupgenius_request( SIGNUP_REPORT, eventid_s = event[ 0 ] )
    if response == False:
        return ( None, signups )
    # print search_results.headers

    try: 
        success = response[ 'success' ]
    except KeyError:
        # we didn't get the expected response so print it out
        print "signupgenius query failed"
        print json.dumps( response, sort_keys = True, indent = 4, separators = (',', ': ') )  
        return ( None, signups )

    if success == False:
        print 'signupgenius returned a failure\n'
        print json.dumps( response, sort_keys = True, indent = 4, separators = (',', ': ') )  
        return ( None, signups )

    signup_response = response[ 'data' ][ 'signup' ]
    # print json.dumps( signup_response, sort_keys = True, indent = 4, separators = (',', ': ') )  
    for signup in signup_response:
        signups.append( ( signup[ 'item' ], signup[ 'firstname' ], signup[ 'lastname' ], signup[ 'email' ], signup[ 'startdatestring' ], signup[ 'enddatestring' ] ) )

    return ( len( signups ), signups )
# end get_signed_up_members


def get_open_slots( event ):
    '''
    returns the number of open slots for each need of the given event
    '''
    needs = []

    # get the response
    response = get_signupgenius_request( SIGNUP_AVAILABLE, eventid_s = event[ 0 ] )
    if response == False:
        return needs
    # print search_results.headers

    try: 
        success = response[ 'success' ]
    except KeyError:
        # we didn't get the expected response so print it out
        print "signupgenius query failed"
        print json.dumps( response, sort_keys = True, indent = 4, separators = (',', ': ') )  
        return needs

    if success == False:
        print 'signupgenius returned a failure\n'
        print json.dumps( response, sort_keys = True, indent = 4, separators = (',', ': ') )  
        return needs

    signup_response = response[ 'data' ]
    # print json.dumps( signup_response, sort_keys = True, indent = 4, separators = (',', ': ') )  
    for need in signup_response:
        try:
            needs.append( ( need[ 'item' ], need[ 'available' ], need[ 'startdatestring' ], need[ 'enddatestring' ] ) )
        except KeyError as err:
            print "Error getting need data: {0}".format( err )
            print json.dumps( response, sort_keys = True, indent = 4, separators = (',', ': ') )  
            continue

    return needs
# end get_open_slots


def get_group_members( group_s ):
    '''
    returns the list of members of the given SUG group
    '''
    results = []

    # get the response
    response = get_signupgenius_request( GROUP_MEMBERS, groupid_s = group_s )
    if response == False:
        return False
    # print search_results.headers

    try: 
        success = response[ 'success' ]
    except KeyError:
        # we didn't get the expected response so print it out
        print "signupgenius query failed"
        print json.dumps( response, sort_keys = True, indent = 4, separators = (',', ': ') )  
        return results

    if success == False:
        print 'signupgenius returned a failure\n'
        print json.dumps( response, sort_keys = True, indent = 4, separators = (',', ': ') )  
        return results

    data = response[ 'data' ]
    print "number of group members: {0}".format( len( data[ 'members' ] ) )
    for member in data[ 'members' ]:
        results.append( ( member[ 'id' ],  member[ 'firstname' ], member[ 'lastname' ] ) )

    return results
# end get_group_members


def write_json_file( event_data ):
    '''
    writes the JSON file corresponding to the given event data summary
    '''
    filename = SIGNUP_SUMMARY_PATH + event_data[ 'event_id' ]

    try:
        f = open( filename, 'w' )
    except IOError as (errno, strerror):
        print( "Could not open output file: {0} - {1}".format( filename, strerror ) )
        raise
    except OSError as err:
        print( "Could not open output file: {0} - {1}".format( filename, err ) )
        raise

    try:
        json.dump( event_data, f, indent=4 )
    except TypeError as err:
        print "Failed to dump to JSON format: {0}".format( err )
        f.close()
        raise

    f.write( "\n" )
    f.close()
# end write_json_file


def write_signupgenius_events( event_reports ):
    '''
    writes a JSON event summary file for each event in the given list
    '''
    report_l = []
    for event_data in event_reports:
        # package the signed up members
        member_l = []
        for ( need, firstname, lastname, email, starttime, endtime ) in event_data[ 2 ]:
            member_l.append( ( {'need':need, 'first_name':firstname, 'last_name':lastname, 'email':email, 'start_time':starttime, 'end_time':endtime} ) )

        # package the open slots
        slot_l = []
        total_open = 0;
        for ( need, number_open, starttime, endtime ) in event_data[ 3 ]:
            total_open = total_open + number_open
            slot_l.append( ( {'need':need, 'start_time':starttime, 'end_time':endtime, 'unfilled_slots':number_open} ) )

        # write the json file passing the dictionary
        event_dict = {'event_id':event_data[ 0 ][ 0 ], 
                      'event_name':event_data[ 0 ][ 1 ], 
                      'number_filled':event_data[ 1 ], 
                      'number_open':total_open,
                      'signed_members':member_l,
                      'open_slots':slot_l}

        write_json_file( event_dict )

        # append this event st we can dump the whole list to output
        report_l.append( event_dict )


    # output the results                                                                                                                           
    # print json.dumps( report_l, sort_keys = True, indent = 4, separators = (',', ': ') )
# end write_signupgenius_events



# start of script

# get all active events
events = get_active_events()
if events == False:
    print "Get active signups failed\n"
    exit()

# members = get_group_members( '4046175' )

# for each active event, collect the data for the members who have signed up and for the still available slots
event_reports = []
for event in events:
    # get the members who have signed up for a slot of some event need
    ( number_filled, signed_members ) = get_signed_up_members( event )

    # get the still open slots
    event_needs = get_open_slots( event )

    # add this event report to the list
    event_reports.append( ( event, number_filled, signed_members, event_needs ) )


# display a completion message
print( "Found {0} SignUpGenius events".format( len( events ) ) )
if len( events ) == 0:
    exit()

# write the results out to an event summary file in JSON format, one per event
try:
    write_signupgenius_events( event_reports )
except ( IOError, OSError, TypeError ):
    # error message is displayed by write_active_leaked_keys()
    exit()

# display a completion message
print( "Wrote SignUpGenius events to " + SIGNUP_SUMMARY_PATH )























get_active_signups.pyOpen
Displaying get_active_signups.py.
