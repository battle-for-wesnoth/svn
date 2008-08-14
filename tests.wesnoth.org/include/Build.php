<?php
/*
   Copyright (C) 2008 by Pauli Nieminen <paniemin@cc.hut.fi>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

class Build {
	private $db;
	private $id;
	private $svn_version;
	private $time;
	private $status;
	private $error_msg;

	private $binary_name;
	private $previous_id;
	private $result;
	private $errors;

	const S_GOOD 	= 0;
	const S_ERROR 	= 1;

	function __construct($revision = -1)
	{
		global $db;
		$this->db = $db;
		$this->binary_name = false;
		$this->previous_id = -1;
		$this->result = null;
		$this->errors = null;
		if ($revision >= 0)
			$this->fetch("WHERE svn_version=?", array($revision));
	}

	private function fetch($where, $params = array())
	{
		$result = $this->db->Execute('SELECT * FROM builds ' . $where . ' LIMIT 1',$params);
		if ($result !== false && !$result->EOF())
		{
			$this->init($result->fields);
		} else {
			$this->reset();
		}
	}

	private static function multiFetch($where, $params = array())
	{
		global $db;
		$res = array();
		$id_list = array();
		$result = $db->Execute('SELECT * FROM builds ' . $where ,$params);
		if ($result === false)
			return $res;
		while (!$result->EOF())
		{			
			$build = new Build();
			$build->init($result->fields);
			$id_list[] = $build->getId();
			$res[] = $build;
			$result->moveNext();
		} 
		return $res;
	}

	public function fetchLast()
	{
		$this->fetch('WHERE id=(SELECT MAX(id) FROM builds)');
	}

	public function fetchBuildById($id)
	{
		$this->fetch('WHERE id=?', array($id));
	}

	private static function fetchVisibleBuilds($page, $builds_per_page)
	{
		$ret = self::multiFetch('ORDER BY id DESC LIMIT ?,?', 
			array(($page-1)*$builds_per_page, $builds_per_page));

		TestResult::fetchResultsForBuilds($ret);
		return $ret;
	}

	private static function getNumberOfVisiblePages($builds_per_page)
	{
		global $db;
		$result = $db->Execute('SELECT COUNT(*) as number FROM builds');
		return ceil($result->fields['number']/$builds_per_page);
	}

	public function init($values)
	{
		foreach($values as $key => $value)
		{
			$this->$key = $value;
		}
		$this->time = $this->db->UnixTimeStamp($this->time);
		$this->result = null;
		$this->errors = null;
	}

	public function reset()
	{
		$this->init(array('id' 			=> -1,
						  'svn_version'	=> -1,
					  	  'time'		=> $this->db->DBTimeStamp(0),
						  'status'		=> -1,
						  'error_msg'	=> ""));
		$this->previous_id = -1;
	}

	public function exists()
	{
		return $this->id > -1;
	}

	public function getId()
	{
		return $this->id;
	}

	public function getStatus()
	{
		return $this->status;
	}

	public function getPreviousId()
	{
		if ($this->previous_id == -1)
		{
			$result = $this->db->Execute('SELECT MAX(id) as previous_id 
				FROM builds 
				WHERE id<?
				AND status=?', 
				array($this->id,
					  self::S_GOOD));
			$this->previous_id = $result->fields['previous_id'];
		}
		return $this->previous_id;
	}
	public function getLastWorkingId()
	{
		if ($this->status == self::S_GOOD)
		{
			return $this->id;
		}
		return $this->getPreviousId();
	}

	public function compile($revision)
	{
		$compiler_log = shell_exec('scons test 2>&1');
		$m =array();
		$this->error_msg = '';
		$this->status = self::S_GOOD;
		if (strpos($compiler_log, "`test' is up to date.") !== false)
		{
			return false;
		}
		// build/debug/editor2/
		$compiler_log = FilenameConverter::stripBuildDirs($compiler_log);
		if (preg_match_all('/^.*(error:|warning:|note:|undefined reference|ld returned \d exit status).*$/mi',$compiler_log, $m,PREG_SET_ORDER))
		{

			foreach($m as $match)
			{
				$this->error_msg .= $match[0] . "\n";
			}
			if (strpos($this->error_msg,'error') !== false 
				|| strpos($this->error_msg,'ld returned'))
				$this->status = self::S_ERROR;
		}

		if(preg_match('/test to \.\/([a-zA-Z0-9_-]*)/',$compiler_log, $m))
		{
			$this->binary_name = $m[1];
		}

		$this->time = time();
		$this->svn_revision = $revision;

		$this->db->StartTrans();

		$this->insert();
		return $this->status == self::S_GOOD;
	}

	public function getTestName()
	{
		return $this->binary_name;
	}

	private function insert()
	{
		$result = $this->db->Execute('INSERT INTO builds 
			(svn_version, status, error_msg) 
			VALUES (?, ?, ?)',
				array($this->svn_revision,
				  	  $this->status,
					  $this->error_msg));
	
		$this->id = $this->db->Insert_ID();
	}

	public function insertNull()
	{
		if ($result = $this->db->Execute('SELECT id FROM builds WHERE id=?', array(0)))
		{
			if ($result->EOF())
			{
				$result = $this->db->Execute('INSERT INTO builds 
					(svn_version, status, error_msg) 
					VALUES (?, ?, ?)',
						array(0,
						  	  self::S_GOOD,
							  ''));
				$this->id = $this->db->Insert_ID();

				$this->db->Execute('UPDATE builds SET id=? WHERE id=?', array(0, $this->id));

			}
		}
	}

	private function checkResult()
	{
		if (is_null($this->result))
		{
			$this->result = new TestResult($this->id);
		}
	}

	public function setResult(TestResult &$result)
	{
		$this->result = $result;
	}

	private function getErrorStatistics()
	{
		$ret = array();
		foreach($this->errors as $err)
		{
			$ret[] = $err->getStatistics();
		}
		return $ret;
	}

	public static function getVisibleBuilds(ParameterValidator $user_params)
	{
		$ret = array();
		$builds_per_page = 18; // TODO: get from config
		$number_of_vissible_pages = 5;
		$page = $user_params->getInt('page', 1);
		$number_of_pages	= self::getNumberOfVisiblePages($builds_per_page);

		$pager = new Paginate('build_history.php?page=', $page, $number_of_pages, $number_of_vissible_pages);

		$ret['paginate'] = $pager->make();

		$ret['builds'] = array();
		$builds = self::fetchVisibleBuilds($ret['paginate']['page'], $builds_per_page);
		foreach($builds as $build)
		{
			$ret['builds'][] = $build->getBuildStats();
		}
		return $ret;
	}

	private function getBuildStats()
	{
		$this->checkResult();

		$build_result = '';
		if ($this->status == self::S_GOOD)
		{
			$build_result = "Build successed." //. " Kaak Test Test"
				; 
		} else {
			$build_result = "Build failed:\n" . $this->error_msg;
		}
		$build_result = str_replace("\n"," \\n",$build_result);

	
		return array('result' 			=> $build_result,
					 'time' 			=> $this->time,
					 'style' 			=> ($this->status == self::S_GOOD?"passed":"failed"),
					 'id'				=> $this->id,
					 'result_style'		=> $this->result->getResult(),
					 'error_msg'		=> $this->error_msg,
					 'svn_rev'			=> $this->svn_version,
					 'result_passed'	=> $this->result->getAssertionsPassed(),
					 'result_failed'	=> $this->result->getAssertionsFailed());
	}

	public function getStatistics()
	{
		if (is_null($this->errors))
			$this->errors = TestError::getErrorsForBuild($this->getLastWorkingId());

		return 	array('builds' => array($this->getBuildStats()),
					  'errors'			=> $this->getErrorStatistics());
	}

	public function pruneOldBuilds()
	{
		$query = 'SELECT b1.id 
				  FROM builds b1 LEFT JOIN test_results as r1 ON b1.id=r1.build_id
								 LEFT JOIN test_errors as e1 ON b1.id=e1.before_id
		  						 LEFT JOIN test_errors as e2 ON b1.id=e1.last_id,
					   builds b2 LEFT JOIN test_results as r2 ON b2.id=r2.build_id
				  WHERE b1.time < ?
					   AND b1.status = b2.status 
				  	   AND b1.error_msg = b2.error_msg 
					   AND (b1.id = b2.id+1)
					   AND (e1.id IS NULL AND b1.id IS NOT NULL)
					   AND ((r1.id IS NULL AND r2.id IS NULL) 
					   		OR (r1.result = r2.result 
								AND r1.assertions_passed=r2.assertions_passed 
								AND r1.assertions_failed=r2.assertions_failed))';

		$_2dayes = 2*24*3600;  // TODO: get from Config
		$result = $this->db->Execute($query, array($this->db->DBTimeStamp(time()-$_2dayes)));
//		var_dump($result->GetAll());
	}
}
?>
