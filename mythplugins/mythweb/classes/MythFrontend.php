<?php
/**
 * Connection routines for the new socket interface to mythfrontend.
 *
 * @url         $URL$
 * @date        $Date$
 * @version     $Revision$
 * @author      $Author$
 * @license     GPL
 *
 * @package     MythTV
 *
/**/

/**
 * A connection to a particular frontend
/**/
class MythFrontend {

/** @var resource   File pointer to the socket connection. */
    private $fp;

/** @var bool       Connected?. */
    private $connected = false;

/** @var string     Hostname to connect to. */
    private $host;
/** @var string     Hostname to connect to. */
    private $port;

/** @var array      List of jump points available on this host. */
    private $jump_points = array();

    public static function findFrontends() {
        global $db;
        $frontends = array();
        $frontends_sh = $db->query('SELECT DISTINCT settings.hostname
                                      FROM settings
                                     WHERE settings.hostname IS NOT NULL
                                       AND settings.value = "NetworkControlEnabled"
                                       AND settings.data  = 1');
        while ( $host = $frontends_sh->fetch_col()) {

        // Remove some characters that should never be here, anyway, and might
        // confuse javascript/html
            $host = preg_replace('/["\']+/', '', $host);

            $port = setting('NetworkControlPort', $host);
            $frontend = new MythFrontend($host, $port);
            $frontend->connect(2);
            if ($frontend->query_location() == 'OFFLINE')
                continue;
            $frontends[$host] = $frontend;
        }
        return $frontends;
    }

/**
 * Object constructor
 *
 * @param string $host Hostname or IP for this frontend.
 * @param int    $port TCP port to connect to.
/**/
    public function __construct($host, $port) {
        $this->host = $host;
        $this->port = $port;
    }

/**
 * Disconnect when destroying the object.
/**/
    function __destruct() {
       $this->disconnect();
   }

/**
 * Open a connection to this frontend.  This also serves as a ping to verify
 * that a frontend is running.
 *
 * @param int $timeout Connection timeout, set low in case the host isn't
 *                     actually on.
/**/
    public function connect($timeout = 5) {
        if ($this->fp)
            return true;
    // Connect.
        $this->fp = @fsockopen($this->host, $this->port, $errno, $errstr, $timeout);
        if ($this->fp === false)
            return false;
    // Read the waiting data.
        $data = $this->get_response(null, true);
        if (strstr($data, '#'))
            return $this->connected = true;
    // Something went wrong (returns false)
        return $this->disconnect();
    }

/**
 * Disconnect from this frontend
/**/
    private function disconnect() {
        if ($this->fp) {
            $this->get_response("exit\n");
            fclose($this->fp);
            $this->fp = null;
        }
        return $this->connected = false;
    }

    public function getHost() {
        return $this->host;
    }

/**
 * Get the response to a query as a string, or an awaiting response buffer if
 * $query is null.
 *
 * @param string $query The query to send, or null
 *
 * @return string The response to $query
/**/
    private function get_response($query, $keep_hash=false) {
        if (!$this->connect())
            return null;
    // Write some data?
        if (!is_null($query))
            fputs($this->fp, preg_replace('/\s*$/', "\n", $query));
    // Get some results
        $recv = '';
        while (true) {
            $data = fread($this->fp, 1024);
            $recv .= $data;
            if (strlen($data) < 1 || strstr($data, '#'))
                break; // EOF
        }
        if ($keep_hash)
            return rtrim($recv);
        else
            return preg_replace('/[\s#]*$/', '', $recv);
    }

/**
 * Call _get_response and return its string broken into an array.
 *
 * @param string $query The query to send, or null
 *
 * @return array The response to $query, broken into an array
/**/
    private function get_rows($query, $sep="\n") {
        $r = $this->get_response($query);
        return $r ? explode($sep, $r) : $r;
    }

/**
 * Load and return the jump points available for this frontend.
 *
 * @return array The jump points available for this frontend
/**/
    public function get_jump_points() {
        if (empty($this->jump_points) || !is_array($this->jump_points)) {
            $this->jump_points = array();
            foreach ($this->get_rows('help jump') as $line) {
                if (preg_match('/(\w+)\s+- (.*)/', $line, $matches)) {
                    $this->jump_points[$matches[1]] = $matches[2];
                }
            }
        }
        return $this->jump_points;
    }

/**
 * Return the location, or a status message that the frontend isn't running.
 *
 * @return string Location of the frontend.
/**/
    public function query_location() {
        $ret = $this->get_response('query location');
        if (empty($ret))
            return 'OFFLINE';
        return $ret;
    }

/**
 * Send a query to the frontend
 *
 * @param string $query The key to send.
/**/
    public function query($query) {
        $lines = $this->get_rows("query $query");
        return $lines;
    }

/**
 * Send a keypress to the frontend
 *
 * @param string $jump_point The key to send.
/**/
    public function send_key($key) {
        $lines = $this->get_rows("key $key");
        if (trim($lines[0]) == 'OK')
            return true;
        return false;
    }

/**
 * Send the frontend to a specific jump point.
 *
 * @param string $jump_point The jump point to send.
/**/
    public function send_jump($jump_point) {
        $lines = $this->get_rows("jump $jump_point");
        if (trim($lines[0]) == 'OK')
            return true;
        return false;
    }

/**
 * Send a playback command to the frontend.
 *
 * @param string $options The playback parameters to pass in.
/**/
    public function send_play($options) {
        $lines = $this->get_rows("play $options");
        if (trim($lines[0]) == 'OK')
            return true;
        return false;
    }

    public function play_program($chanid, $starttime) {
        $starttime = date('Y-m-d\TH:i:s', $starttime);
        return $this->send_play("program $chanid $starttime resume");
    }
}
